#define __GNU_SOURCE

#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <numaif.h>
#elif defined(__sun__)
#include <sys/pset.h>
#endif


#define liblock_allocate malloc


struct topology {
    struct core_node* nodes;
    struct core*      cores;
    int               nb_nodes;
    int               nb_cores;
};

struct core { // Misnomer, actually a hardware thread
    struct core_node* node;
    int               core_id;
    float             frequency;
    const char*       server_type; /* 0 => free, "client" => client, other => a server */
};

struct core_node { // NUMA node
    struct core** cores;
    int           node_id;
    int           nb_cores;
};

struct topology real_topology;
struct topology* topology = &real_topology;
#ifdef __linux__
static cpu_set_t client_cpuset;
#elif defined(__solaris__)
#endif

#ifdef __linux__
#define GET_NODES_CMD \
    "NODES=/sys/devices/system/node;" \
    "CPUS=/sys/devices/system/cpu;" \
    "if [ -d $NODES ]; then" \
    "  for F in `ls -d $NODES/node*`; do " \
    "    if [ $(cat $F/cpulist | grep '-') ]; then " \
    "      seq -s ' ' $(cat $F/cpulist | tr -s '-' ' '); " \
    "    else" \
    "      cat $F/cpulist | tr -s ',' ' ';" \
    "    fi; " \
    "  done;" \
    "else" \
    "  find $CPUS -maxdepth 1 -name \"cpu[0-9]*\" -exec basename {} \\; | " \
    "  sed -e 's/cpu\\(.*\\)/\\1/g' | tr -s '\\n' ' ';" \
    "fi"

#define GET_FREQUENCIES_CMD \
    "cat /proc/cpuinfo | grep \"cpu MHz\" | sed -e 's/cpu MHz\\t\\t://'"
#elif defined(__sun__)
#define GET_NODES_CMD \
   "for COND in `psrinfo -pv | " \
   "             sed -n 's/[^(]*(\\(.*\\)-\\(.*\\))$/i=\\1;i\\<\\=\\2/p'`; do" \
   "  nawk 'BEGIN{ first=1; for('$COND';i++) " \
   "                          { printf \"%s%s\",(first?\"\":\" \"),i; first=0;}}'; " \
   "  echo;" \
   "done"

#define GET_FREQUENCIES_CMD \
   "kstat cpu_info | grep 'clock_MHz' | sed -n 's/[^0-9]*\\([0-9]*\\).*/\\1/p'"
#endif

// for COND in `psrinfo -pv |              sed -n 's/[^(]*(\(.*\)-\(.*\))$/i=\1\;i\<\=\2/p'`; do nawk 'BEGIN{ first=1; for('$COND';i++) { printf "%s%s",first?"":" ",i; first=0;}}';   echo;done 

   
static void extract_topology(const char* cmd_nodes,
                             const char* cmd_frequencies);
void fatal(char *message);


int main(void)
{
    int i, j;
    struct core_node node;
    struct core core;

#ifdef __linux__
    char *os = "Linux";
#elif defined(__sun__)
    char *os = "Solaris";
#endif

    extract_topology(GET_NODES_CMD, GET_FREQUENCIES_CMD);
    
    printf("Operating system: %s\n", os);
    printf("Number of nodes: %d\n", topology->nb_nodes);
    printf("Number of cores: %d\n", topology->nb_cores);

    printf("\n");
   
    for (i = 0; i < topology->nb_nodes; i++)
    {
        node = topology->nodes[i];

        printf("NUMA Node %d: id = %d (%d thread%s)\n",
               i, node.node_id, node.nb_cores, node.nb_cores > 1 ? "s" : "");
        
        for (j = 0; j < node.nb_cores; j++)
        {
            core = *(node.cores[j]);
            
            printf("  Hardware context %d: id = %d [%.2fMhz]\n",
                   j, core.core_id, core.frequency);
        }
    }


/*
    printf("\n");

    for (i = 0; i < topology->nb_cores; i++)
    {
        core = topology->cores[i];
            
        printf("  Core %d: id = %d [%.2fMhz] (%d hardware context%s)\n",
               i, core.core_id, core.frequency, 1,
               1 > 1 ? "s" : "");
    }

    printf("\n");

    printf("GET_NODES_CMD:\n%s\n\n", GET_NODES_CMD);
    printf("GET_FREQUENCIES_CMD:\n%s\n", GET_FREQUENCIES_CMD);
*/

    
    return;
}


static void extract_topology(const char* cmd_nodes,
                             const char* cmd_frequencies) {
    FILE *file;
    char  text[1024], *p, *saveptr;
    int   cores_per_nodes[1024];
    int   i;
    int   nb_nodes = 0;
    int   nb_cores = 0;
    const char* ld = getenv("LD_PRELOAD");

    unsetenv("LD_PRELOAD");

    /* We use UNIX commands to find the nodes. */
    if(!(file = popen(cmd_nodes, "r")))
        fatal("popen");

    /* For each node... */
    while(fgets(text, 1024, file)) {
        for(i=0, p = strtok_r(text, " ", &saveptr); p && strlen(p); p = strtok_r(0, " ", &saveptr))
            i++;

        cores_per_nodes[nb_nodes] = i;
        nb_nodes++;
        nb_cores += i;
    }

    if (pclose(file) < 0)
        fatal("pclose");

    topology->nodes = liblock_allocate(nb_nodes * sizeof(struct core_node));
    topology->cores = liblock_allocate(nb_cores * sizeof(struct core)); 
    topology->nb_nodes = nb_nodes;
    topology->nb_cores = nb_cores;

    for(i=0; i<nb_cores; i++) {
#ifdef __linux__
        CPU_SET(i, &client_cpuset);
#elif defined(__sun__)
#endif
        topology->cores[i].core_id = i;
        topology->cores[i].server_type = 0;
    }

    /* We use UNIX commands to find the nodes. */
    if(!(file = popen(cmd_nodes, "r")))
        fatal("popen");

    nb_nodes = 0;
    /* For each node... */
    while(fgets(text, 1024, file)) {
        struct core_node* node = &topology->nodes[nb_nodes];

        node->nb_cores          = cores_per_nodes[nb_nodes];
        node->cores             = liblock_allocate(node->nb_cores*sizeof(struct core_node*));
        node->node_id           = nb_nodes++;

        for(i=0, p = strtok_r(text, " ", &saveptr); p && strlen(p); p = strtok_r(0, " ", &saveptr), i++) {
            struct core* core = &topology->cores[atoi(p)];
            node->cores[i] = core;
            core->node = node;
        }
    }

    if (pclose(file) < 0)
        fatal("pclose");

    file = popen(cmd_frequencies, "r");

    if (file == NULL)
        fatal("popen");

    nb_cores = 0;
    while (fgets(text, 1024, file) != NULL)
        topology->cores[nb_cores++].frequency = atof(text);

    if (pclose(file) < 0)
        fatal("pclose");

    if(ld)
        setenv("LD_PRELOAD", ld, 1);
}

void fatal(char *message)
{
    fprintf(stderr, "FATAL: %s\n", message);
    exit(-1);
}

