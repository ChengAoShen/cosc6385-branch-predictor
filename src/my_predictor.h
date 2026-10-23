// my_predictor.h
// COSC6385 Branch Predictor Assignment -- ChengAo Shen (ID: 2437132)
// This file contains a sample gshare_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.

class gshare_update : public branch_update {
public:
	unsigned int index;
};

class gshare_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	gshare_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];

	gshare_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((gshare_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};

//
// Pentium M hybrid branch predictors
// This class implements a simple hybrid branch predictor based on the Pentium M branch outcome prediction units. 
// Instead of implementing the complete Pentium M branch outcome predictors, the class below implements a hybrid 
// predictor that combines a bimodal predictor and a global predictor. 
// Configuration (each value can be overridden with -D at compile time)
#ifndef PM_BIM_BITS
#define PM_BIM_BITS	12	// bimodal table: 2^12 = 4096 2-bit counters
#endif
#ifndef PM_SET_BITS
#define PM_SET_BITS	9	// global table: 2^9 = 512 sets
#endif
#ifndef PM_WAYS
#define PM_WAYS		4	// ways per set (Pentium M uses 4)
#endif
#ifndef PM_TAG_BITS
#define PM_TAG_BITS	10	// partial tag width
#endif
#ifndef PM_HIST_LEN
#define PM_HIST_LEN	21	// global history register length
#endif
#ifndef PM_FOLD
#define PM_FOLD		1	// 1: fold history longer than set+tag bits into the hash
#endif
#ifndef PM_ALLOC
#define PM_ALLOC	1	// 0: allocate on every global miss
				// 1: allocate on global miss only if bimodal mispredicted
#endif

class pm_update : public branch_update {
public:
	unsigned int index;	// bimodal index
	unsigned int set;	// global set (row)
	unsigned int tag;	// global tag
	int way;		// hit way, or -1 on global miss
	bool bim_pred;		// bimodal prediction
};

class pm_predictor : public branch_predictor {
public:
	// one way of the tagged global predictor
	struct pm_entry {
		bool valid;
		unsigned short tag;
		unsigned char ctr;	// 2-bit saturating counter
		unsigned char lru;	// age: 0 = most recently used
	};

	pm_update u;
	branch_info bi;
	unsigned int history;
	unsigned char bim[1<<PM_BIM_BITS];
	pm_entry glob[1<<PM_SET_BITS][PM_WAYS];

	pm_predictor (void) : history(0) {
		// bimodal counters start at weakly not taken
		memset (bim, 1, sizeof (bim));
		for (int s = 0; s < (1<<PM_SET_BITS); s++)
			for (int w = 0; w < PM_WAYS; w++) {
				glob[s][w].valid = false;
				glob[s][w].tag = 0;
				glob[s][w].ctr = 1;
				glob[s][w].lru = w;
			}
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			// bimodal: index by low address bits
			u.index = b.address & ((1<<PM_BIM_BITS)-1);
			u.bim_pred = bim[u.index] >> 1;

			// global: hash = address XOR history,
			// low bits select the set, next bits form the tag
			unsigned int h = b.address ^ history;
			if (PM_FOLD)
				h ^= history >> (PM_SET_BITS + PM_TAG_BITS);
			u.set = h & ((1<<PM_SET_BITS)-1);
			u.tag = (h >> PM_SET_BITS) & ((1<<PM_TAG_BITS)-1);
			u.way = -1;
			for (int w = 0; w < PM_WAYS; w++) {
				pm_entry &e = glob[u.set][w];
				if (e.valid && e.tag == u.tag) {
					u.way = w;
					break;
				}
			}

			// global hit overrides bimodal
			if (u.way >= 0)
				u.direction_prediction (glob[u.set][u.way].ctr >> 1);
			else
				u.direction_prediction (u.bim_pred);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	// make way w the most recently used way of set s
	void touch (unsigned int s, int w) {
		unsigned char old = glob[s][w].lru;
		for (int i = 0; i < PM_WAYS; i++)
			if (glob[s][i].lru < old) glob[s][i].lru++;
		glob[s][w].lru = 0;
	}

	static void count (unsigned char *c, bool taken) {
		if (taken) {
			if (*c < 3) (*c)++;
		} else {
			if (*c > 0) (*c)--;
		}
	}

	void update (branch_update *bu, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			pm_update *pu = (pm_update*)bu;

			// bimodal is always updated
			count (&bim[pu->index], taken);

			if (pu->way >= 0) {
				// global hit: train the counter, update LRU
				count (&glob[pu->set][pu->way].ctr, taken);
				touch (pu->set, pu->way);
			} else if (PM_ALLOC == 0 || pu->bim_pred != taken) {
				// global miss: replace the LRU way
				int victim = 0;
				for (int w = 0; w < PM_WAYS; w++)
					if (glob[pu->set][w].lru == PM_WAYS-1) victim = w;
				pm_entry &e = glob[pu->set][victim];
				e.valid = true;
				e.tag = pu->tag;
				e.ctr = taken ? 2 : 1;
				touch (pu->set, victim);
			}

			history <<= 1;
			history |= taken;
			history &= (1<<PM_HIST_LEN)-1;
		}
	}
};

//
// Complete Pentium M branch predictors for extra credit
// This class implements the complete Pentium M branch prediction units. 
// It implements both branch target prediction and branch outcome predicton. 
class cpm_update : public branch_update {
public:
        unsigned int index;
};

class cpm_predictor : public branch_predictor {
public:
        cpm_update u;

        cpm_predictor (void) {
        }

        branch_update *predict (branch_info & b) {
            u.direction_prediction (true);
            u.target_prediction (0);
            return &u;
        }

        void update (branch_update *u, bool taken, unsigned int target) {
        }

};


