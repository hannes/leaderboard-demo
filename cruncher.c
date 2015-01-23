#include "khash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

#define QUERY_FIELD_QID 0
#define QUERY_FIELD_A1 1
#define QUERY_FIELD_A2 2
#define QUERY_FIELD_A3 3
#define QUERY_FIELD_A4 4
#define QUERY_FIELD_BS 5
#define QUERY_FIELD_BE 6


KHASH_MAP_INIT_INT(64, char)
void *person_map, *knows_map, *interest_map;
byteoffset person_length, knows_length, interest_length;

int q_id;
int q_artist = 0;
int q_relartists[3] = {0, 0, 0};
int q_bdaystart = 0;
int q_bdayend = 0;

FILE *outfile;

int result_comparator(const void *v1, const void *v2)
{
    struct Result *r1 = (struct Result *) v1;
    struct Result *r2 = (struct Result *) v2;
    int rc;
    if (r1->score > r2->score)
        return -1;
    else if (r1->score < r2->score)
        return +1;
    else if (r1->person_id < r2->person_id)
        return -1;
    else if (r1->person_id > r2->person_id)
        return +1;
     else if (r1->knows_id < r2->knows_id)
        return -1;
    else if (r1->knows_id > r2->knows_id)
        return +1;
    else
        return 0;
}

void query(int qid, int artist, int areltd[], int bdstart, int bdend) {
	byteoffset person_offset, interest_offset, knows_offset;
	struct Person *person;
	struct Person *knows;
	char score, likesartist;
	long interest;
	
	khash_t(64) *person_scores;
	person_scores = kh_init(64);
	khiter_t hash_iter;

	int result_length = 0, result_idx, ret;

	printf("Running query %d\n", qid);

	// scan people, filter by birthday, calculate scores, add to hash map
	for (person_offset = 0; person_offset < person_length; person_offset += sizeof(struct Person)) {
		person = person_map + person_offset;

		if (person->birthday < bdstart || person->birthday > bdend) {
			continue;
		}
		
		score = 0;
		for (interest_offset = person->interests_first; 
			interest_offset < person->interests_first + person->interest_n * sizeof(long); 
			interest_offset += sizeof(long)) {

			interest = *((long *) (interest_map + interest_offset));
			if (areltd[0] == interest) score++;
			if (areltd[1] == interest) score++;
			if (areltd[2] == interest) score++;
		}
		if (score > 0) {
			kh_val(person_scores, kh_put(64, person_scores, person_offset, &ret)) = score;
		}
	}

	// prepare array to hold results
	struct Result* results = malloc(1000 * sizeof (struct Result));

	// scan hash map and find friends
	for (hash_iter = kh_begin(person_scores); hash_iter != kh_end(person_scores); ++hash_iter) {
		if (!kh_exist(person_scores, hash_iter)) continue;

		person = person_map + kh_key(person_scores, hash_iter);
		score = kh_value(person_scores, hash_iter);

		for (knows_offset = person->knows_first; 
			knows_offset < person->knows_first + person->knows_n * sizeof(long); 
			knows_offset += sizeof(long)) {
			knows = person_map + *((byteoffset *) (knows_map + knows_offset));

			// check if friend lives in same city and likes artist 
			if (person->location != knows->location) continue; 

			likesartist = 0;
			for (interest_offset = knows->interests_first; 
				interest_offset < knows->interests_first + knows->interest_n * sizeof(long); 
				interest_offset += sizeof(long)) {

				interest = *((long *) (interest_map + interest_offset));
				if (interest == artist) {
					likesartist = 1;
					break;
				}
			}
			// TODO: realloc if we run out of space in results...
			// add to result set
			if (likesartist) {
				results[result_length].person_id = person->person_id;
				results[result_length].knows_id = knows->person_id;
				results[result_length].score = score;
				result_length++;
			}
		}
	}

	// sort result
	qsort(results, result_length, sizeof(struct Result), &result_comparator);

	// output
	for (result_idx = 0; result_idx < result_length; result_idx++) {
		fprintf(outfile, "%d|%d|%lu|%lu\n", qid, results[result_idx].score, 
			results[result_idx].person_id, results[result_idx].knows_id);
	}
}

void query_field_handler(int col, char* field) {
	switch(col) {
		case QUERY_FIELD_QID:
			q_id = atoi(field);
			break;
		case QUERY_FIELD_A1:
			q_artist = atoi(field);
			break;
		case QUERY_FIELD_A2:
			q_relartists[0] = atoi(field);
			break;
		case QUERY_FIELD_A3:
			q_relartists[1] = atoi(field);
			break;
		case QUERY_FIELD_A4:
			q_relartists[2] = atoi(field);
			break;
		case QUERY_FIELD_BS:
			q_bdaystart = birthday_to_short(field);
			break;
		case QUERY_FIELD_BE:
			q_bdayend = birthday_to_short(field);
			break;
		default:
			return;
	}
}

void query_line_finisher() {
	query(q_id, q_artist, q_relartists, q_bdaystart, q_bdayend);
}


int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr, "Usage: [datadir] [query file] [results file]\n");
		exit(1);
	}
	/* memory-map files created by loader */
	person_map =   mmapr(makepath(argv[1], "person",   "bin"), &person_length);
	interest_map =    mmapr(makepath(argv[1], "interest", "bin"), &knows_length);
	knows_map = mmapr(makepath(argv[1], "knows",    "bin"), &interest_length);

  	outfile = fopen(argv[3], "w");  
  	if (outfile == NULL) {
  		fprintf(stderr, "Can't write to output file at %s\n", argv[3]);
		exit(-1);
  	}

  	/* run through queries */
	parse_csv(argv[2], &query_field_handler, &query_line_finisher);
	return 0;
}

