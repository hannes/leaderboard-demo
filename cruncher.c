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


void *person_map, *knows_map, *interest_map;
byteoffset person_length, knows_length, interest_length;

// query variables
int q_id, q_artist, q_bdaystart, q_bdayend;
int q_relartists[3];

FILE *outfile;

int result_comparator(const void *v1, const void *v2) {
    struct Result *r1 = (struct Result *) v1;
    struct Result *r2 = (struct Result *) v2;
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

short get_score(struct Person *person, int areltd[]) {
	byteoffset interest_offset;
	long interest;
	short score = 0;
	for (interest_offset = person->interests_first; 
		interest_offset < person->interests_first + person->interest_n * sizeof(long); 
		interest_offset += sizeof(long)) {

		interest = *((long *) (interest_map + interest_offset));
		if (areltd[0] == interest) score++;
		if (areltd[1] == interest) score++;
		if (areltd[2] == interest) score++;
	}
	return score;
}

char likes_artist(struct Person *person, int artist) {
	byteoffset interest_offset;
	long interest;
	short likesartist = 0;

	for (interest_offset = person->interests_first; 
		interest_offset < person->interests_first + person->interest_n * sizeof(long); 
		interest_offset += sizeof(long)) {

		interest = *((long *) (interest_map + interest_offset));
		if (interest == artist) {
			likesartist = 1;
			break;
		}
	}
	return likesartist;
}

void query(int qid, int artist, int areltd[], int bdstart, int bdend) {
	byteoffset person_offset, interest_offset, knows_offset;
	struct Person *person;
	struct Person *knows;
	char score;

	int result_length = 0, result_idx, ret, result_set_size = 1000;

	printf("Running query %d\n", qid);

	struct Result* results = malloc(result_set_size * sizeof (struct Result));

	// scan people, filter by birthday, calculate scores, add to hash map
	for (person_offset = 0; person_offset < person_length; person_offset += sizeof(struct Person)) {
		person = person_map + person_offset;

		if (person->birthday < bdstart || person->birthday > bdend) continue; 

		// person must not like artist yet
		if (likes_artist(person, artist)) continue;
		// but person must like some of these other guys
		score = get_score(person, areltd);
		if (score < 1) continue;

		// check if friend lives in same city and likes artist 
		for (knows_offset = person->knows_first; 
			knows_offset < person->knows_first + person->knows_n * sizeof(long); 
			knows_offset += sizeof(long)) {
			knows = person_map + *((byteoffset *) (knows_map + knows_offset));

			if (person->location != knows->location) continue; 

			if (likes_artist(knows, artist)) {
				// realloc result array if we run out of space
				if (result_length >= result_set_size) {
					result_set_size *= 2;
					results = realloc(results, result_set_size * sizeof (struct Result));
				}

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
	person_map   = mmapr(makepath(argv[1], "person",   "bin"), &person_length);
	interest_map = mmapr(makepath(argv[1], "interest", "bin"), &knows_length);
	knows_map    = mmapr(makepath(argv[1], "knows",    "bin"), &interest_length);

  	outfile = fopen(argv[3], "w");  
  	if (outfile == NULL) {
  		fprintf(stderr, "Can't write to output file at %s\n", argv[3]);
		exit(-1);
  	}

  	/* run through queries */
	parse_csv(argv[2], &query_field_handler, &query_line_finisher);
	return 0;
}

