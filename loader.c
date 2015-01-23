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

#define PERSON_FIELD_ID 0
#define PERSON_FIELD_BIRTHDAY 4
#define PERSON_FIELD_LOCATION 8
#define KNOWS_FIELD_PERSON 0
#define KNOWS_FIELD_FRIEND 1
#define INTEREST_FIELD_PERSON 0
#define INTEREST_FIELD_INTEREST 1

KHASH_MAP_INIT_INT(64, byteoffset)
static khash_t(64) *person_offsets;
static FILE * person_out;
static FILE * interest_out;
static FILE * knows_out;
void* person_map;

static struct Person *person;
static long person_id = 0;
static long knows_id = 0;
static long interest_id = 0;
static long person_id_prev = 0;
static byteoffset person_offset = 0;
static byteoffset knows_offset = 0;
static byteoffset interest_offset = 0;

void person_field_handler(int col, char* field) {
	switch(col) {
		case PERSON_FIELD_ID:
			person_id = atol(field);
			person->person_id = person_id;
			break;
		case PERSON_FIELD_BIRTHDAY:
			person->birthday = birthday_to_short(field);
			break;
		case PERSON_FIELD_LOCATION:
			person->location = atol(field);
			break;
		default:
			return;
	}
}

void person_line_finisher() {
	int ret;
	khiter_t k;

	k = kh_put(64, person_offsets, person_id, &ret);
	kh_value(person_offsets, k) = person_offset;
	fwrite(person, sizeof(struct Person), 1, person_out);
	person_offset += sizeof(struct Person);
}

void knows_field_handler(int col, char* field) {
	switch(col) {
		case KNOWS_FIELD_PERSON:
			person_id = atol(field);
			break;
		case KNOWS_FIELD_FRIEND:
			knows_id = atol(field);
			break;
		default:
			return;
	}
}

void updatePerson() {
	person = (person_map + kh_value(person_offsets, 
		kh_get(64, person_offsets, person_id)));
	person_id_prev = person_id;
}

void knows_line_finisher() {
	byteoffset knows_person_offset;
	if (person_id != person_id_prev) {
		updatePerson();
		person->knows_first = knows_offset;
	}
	// lookup other person and write offset
	knows_person_offset = kh_value(person_offsets, 
			kh_get(64, person_offsets, knows_id));
	fwrite(&knows_person_offset, sizeof(byteoffset), 1, knows_out);
	knows_offset += sizeof(byteoffset);
	person->knows_n++;
}

void interest_field_handler(int col, char* field) {
	switch(col) {
		case INTEREST_FIELD_PERSON:
			person_id = atol(field);
			break;
		case INTEREST_FIELD_INTEREST:
			interest_id = atol(field);
			break;
		default:
			return;
	}
}

void interest_line_finisher() {
	if (person_id != person_id_prev) {
		updatePerson();
		person->interests_first = interest_offset;
	}
	fwrite(&interest_id, sizeof(long), 1, interest_out);
	interest_offset+= sizeof(long);
	person->interest_n++;
}



int main(int argc, char *argv[]) {
	char* person_input_file    = makepath(argv[1], "person",   "csv");
	char* interest_input_file  = makepath(argv[1], "interest", "csv");
	char* knows_input_file     = makepath(argv[1], "knows",    "csv");
	char* person_output_file   = makepath(argv[2], "person",   "bin");
	char* interest_output_file = makepath(argv[2], "interest", "bin");
	char* knows_output_file    = makepath(argv[2], "knows",    "bin");
	
	khiter_t k;
	int person_map_fd;

	person = malloc(sizeof(struct Person));
	person->knows_n = 0;
	person->knows_first = 0;
	person->interest_n = 0;
	person->interests_first = 0;

	if (argc < 3) {
		fprintf(stderr, "Usage: [csv input path] [output path]\n");
		exit(-1);
	}

	// init hashmap for persons
	person_offsets = kh_init(64);

	// first pass person, parse person, write to binary and store bin offset in hash table
	person_out = open_binout(person_output_file);
	parse_csv(person_input_file, &person_field_handler, &person_line_finisher);

	// mmap person.bin binary file for updates
	fclose(person_out);

	person_map_fd = open(person_output_file, O_RDWR);
	person_map = mmap(0, person_offset, PROT_READ | PROT_WRITE, 
		MAP_SHARED, person_map_fd, 0);
	if (person_map_fd == 0 || person_map == MAP_FAILED) {
		fprintf(stderr, "Failed to map person binary.\n");
		exit(-1);
	}
	close(person_map_fd);

	// pass through interest and friends, write to binary, set offsets in person
	interest_out = open_binout(interest_output_file);
	parse_csv(interest_input_file, &interest_field_handler, &interest_line_finisher);

	person_id = 0;
	knows_out = open_binout(knows_output_file);
	parse_csv(knows_input_file, &knows_field_handler, &knows_line_finisher);
	return 0;
}

