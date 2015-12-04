#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <complex.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include "tsp-types.h"
#include "tsp-job.h"
#include "tsp-genmap.h"
#include "tsp-print.h"
#include "tsp-tsp.h"
#include "tsp-lp.h"
#include "tsp-hkbound.h"


/* macro de mesure de temps, retourne une valeur en nanosecondes */
#define TIME_DIFF(t1, t2) \
  ((t2.tv_sec - t1.tv_sec) * 1000000000ll + (long long int) (t2.tv_nsec - t1.tv_nsec))


/* tableau des distances */
tsp_distance_matrix_t tsp_distance ={};

struct mult_arg {
	tsp_path_t * sol; 	
	struct tsp_queue *q;
	uint64_t * vpres; 
	long long int * cut; 
	tsp_path_t * s; 
	int * sol_len;
};

pthread_mutex_t mutex_getjob;
//pthread_mutex_t mutex_min;


// Source 
void *f(void *arg){
	struct mult_arg * m = arg; 

    pthread_mutex_lock(&mutex_queue);
    while (!empty_queue (m->q)) {
        pthread_mutex_unlock(&mutex_queue);
        int hops = 0, len = 0;

        //pthread_mutex_lock(&mutex_getjob);
        get_job (m->q, *(m->sol), &hops, &len, m->vpres);
        //pthread_mutex_unlock(&mutex_getjob);
    
        // le noeud est moins bon que la solution courante
        pthread_mutex_lock(&mutex_min);
        if (minimum < INT_MAX && (nb_towns - hops) > 10 && ( (lower_bound_using_hk(*(m->sol), hops, len, *(m->vpres))) >= minimum || (lower_bound_using_lp(*(m->sol), hops, len, *(m->vpres))) >= minimum)){
            pthread_mutex_unlock(&mutex_min);
            pthread_mutex_lock(&mutex_queue);
            continue;
        }
        pthread_mutex_unlock(&mutex_min);

        tsp (hops, len, *(m->vpres), *(m->sol), m->cut, *(m->s), m->sol_len);
    
        pthread_mutex_lock(&mutex_queue);

    }
    pthread_mutex_unlock(&mutex_queue);

    return NULL;
}

/** Paramètres **/

/* nombre de villes */
int nb_towns=10;
/* graine */
long int myseed= 0;
/* nombre de threads */
int nb_threads=1;

/* affichage SVG */
bool affiche_sol= false;
bool affiche_progress=false;
bool quiet=false;

static void generate_tsp_jobs (struct tsp_queue *q, int hops, int len, uint64_t vpres, tsp_path_t path, long long int *cuts, tsp_path_t sol, int *sol_len, int depth)
{
    if (len >= minimum) {
        (*cuts)++ ;
        return;
    }
    
    if (hops == depth) {
        /* On enregistre du travail à faire plus tard... */
      add_job (q, path, hops, len, vpres);
    } else {
        int me = path [hops - 1];        
        for (int i = 0; i < nb_towns; i++) {
	  if (!present (i, hops, path, vpres)) {
                path[hops] = i;
		vpres |= (1<<i);
                int dist = tsp_distance[me][i];
                generate_tsp_jobs (q, hops + 1, len + dist, vpres, path, cuts, sol, sol_len, depth);
		vpres &= (~(1<<i));
            }
        }
    }
}

static void usage(const char *name) {
  fprintf (stderr, "Usage: %s [-s] <ncities> <seed> <nthreads>\n", name);
  exit (-1);
}

int main (int argc, char **argv)
{
    unsigned long long perf;
    tsp_path_t path;
    uint64_t vpres=0;
    tsp_path_t sol;
    int sol_len;
    long long int cuts = 0;
    struct tsp_queue q;
    struct timespec t1, t2;

    /* lire les arguments */
    int opt;
    while ((opt = getopt(argc, argv, "spq")) != -1) {
      switch (opt) {
      case 's':
	affiche_sol = true;
	break;
      case 'p':
	affiche_progress = true;
	break;
      case 'q':
	quiet = true;
	break;
      default:
	usage(argv[0]);
	break;
      }
    }

    if (optind != argc-3)
      usage(argv[0]);

    nb_towns = atoi(argv[optind]);
    myseed = atol(argv[optind+1]);
    nb_threads = atoi(argv[optind+2]);
    assert(nb_towns > 0);
    assert(nb_threads > 0);
   
    minimum = INT_MAX;
      
    /* generer la carte et la matrice de distance */
    if (! quiet)
      fprintf (stderr, "ncities = %3d\n", nb_towns);
    genmap ();

    init_queue (&q);
    // a mertre dans init_queue:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    pthread_mutex_init(&mutex_getjob, NULL);
    pthread_mutex_init(&mutex_min, NULL);
    pthread_mutex_init(&mutex_queue, NULL);
    pthread_mutex_init(&mutex_cuts, NULL);



    clock_gettime (CLOCK_REALTIME, &t1);

    memset (path, -1, MAX_TOWNS * sizeof (int));
    path[0] = 0;
    vpres=1;

    /* mettre les travaux dans la file d'attente */
    generate_tsp_jobs (&q, 1, 0, vpres, path, &cuts, sol, & sol_len, 3);
    no_more_jobs (&q);
   
    /* calculer chacun des travaux */
    tsp_path_t solution;
    memset (solution, -1, MAX_TOWNS * sizeof (int));
    solution[0] = 0;

    // 
    pthread_t tid[4]; 
    int rep;
    struct mult_arg *pi = malloc(sizeof(struct mult_arg));
    pi->sol = &solution;
    pi->q = &q;
    pi->vpres = &vpres; 
    pi->cut = &cuts; 
    pi->s = &sol; 
    pi->sol_len = &sol_len;

    for(int ind = 0; ind < 4; ind ++){
        if ( 0 == (rep = pthread_create(tid+ind, NULL, f,(void *)pi)) ) {
    		printf("Pthread %i creee\n",ind);
    	} else { 
    		fprintf(stderr, "Yo"); perror("<!-- pthread_create -->");
    	}    
    }

    void *status;
	pthread_join(tid[0], &status);
    pthread_join(tid[1], &status);
    pthread_join(tid[2], &status);
    pthread_join(tid[3], &status);
    pthread_mutex_destroy(&mutex_getjob);
    pthread_mutex_destroy(&mutex_min);
    pthread_mutex_destroy(&mutex_queue);
    pthread_mutex_destroy(&mutex_cuts);



    free(pi);

    clock_gettime (CLOCK_REALTIME, &t2);

    if (affiche_sol)
      print_solution_svg (sol, sol_len);

    perf = TIME_DIFF (t1,t2);
    printf("<!-- # = %d seed = %ld len = %d threads = %d time = %lld.%03lld ms ( %lld coupures ) -->\n",
	   nb_towns, myseed, sol_len, nb_threads,
	   perf/1000000ll, perf%1000000ll, cuts);

    return 0 ;
}


