#ifndef _CACHE_SIM_H
#define _CACHE_SIM_H _CACHE_SIM_H
#include <vector>
#include <map>

using namespace std;

typedef struct cache_line_t
{
	bool valid;
	unsigned int tag;
	unsigned int recent;
}cache_line;

typedef struct stat_t
{
	unsigned int hit;
	unsigned int miss;
}stat;

typedef struct print_stat_t
{
	unsigned int pc;
	bool read;
	unsigned long ref;
	unsigned int miss;
	float 	miss_rate;
	unsigned long miss_cycle;
	float	contrib;

	bool operator > (const print_stat_t& ps) const
	{
		return miss > ps.miss;
	}
}print_stat;

class Cache
{
public:
	Cache();
	Cache(unsigned int cache_size, 
		unsigned int line_size, 
		unsigned int miss_penalty, 
		bool is_direct_mapped);
void dataRead(unsigned int addr, unsigned int mem_addr);
void dataWrite(unsigned int addr, unsigned int mem_addr);
	~Cache();

// private:
	unsigned int cache_size;	// in bytes
	unsigned int line_size;	// how many blocks in a line
	unsigned int set_size;	// how many sets are there
	unsigned int assoc;
	unsigned int miss_penalty;

	// unsigned int m;
	unsigned int s;
	unsigned int b;
	unsigned int t;
	vector< vector<cache_line*>* > *cache;

	map<unsigned int, stat> cacheloadstat;
	map<unsigned int, stat> cachestorestat;
	map<unsigned int, stat> cache_print_stat;
	vector<print_stat> print_vector;
	map<unsigned int, unsigned int> pc2addr;

};

#endif
