#define REPORTING_N 1000000
#define LINEBUFLEN 1024

typedef unsigned long byteoffset;
typedef unsigned int  entrycount;

typedef struct {
	unsigned long person_id;
	unsigned short birthday;
	unsigned short location;
	unsigned long knows_first;
	unsigned short knows_n;
	unsigned int interests_first; // interests fit in uint
	unsigned short interest_n;
} Person;

typedef struct { 
    unsigned long person_id;
    unsigned long knows_id;
    unsigned char score;
} Result;

void parse_csv(char* fname, void (*line_handler)(unsigned char nfields, char** fieldvals)) {
	long nlines = 0;

   	FILE* stream = fopen(fname, "r");
	if (stream == NULL) {
		fprintf(stderr, "Can't read file at %s\n", fname);
		exit(-1);
	}
	char line[LINEBUFLEN];
	char* tokens[10];
	unsigned int col, idx;
	tokens[0] = line;

	while (fgets(line, LINEBUFLEN, stream)) {
		col = 0;
		// parse the csv line into array of strings
		for (idx=0; idx<LINEBUFLEN; idx++) { 
			if (line[idx] == '|' || line[idx] == '\n') {
				line[idx] = '\0';
				col++;
				tokens[col] = &line[idx+1];
			} // lookahead to find end of line
			if (line[idx+1] == '\0') {
				break;
			}
		}
		(*line_handler)(col, tokens);
		nlines++;
		if (nlines % REPORTING_N == 0) {
			printf("%s: read %lu lines\n", fname, nlines);
		}
	}
	fclose(stream);
}


FILE* open_binout(char* filename) {
	FILE* outfile;
	outfile = fopen(filename, "wb");
	if (outfile == NULL) {
		fprintf(stderr, "Could not open %s for writing\n", filename);
		exit(-1);
	}
	return outfile;
}

/* 
	convert birthday from date to four-digit integer with month and day 
	easier comparisions and less storage space
*/
unsigned short birthday_to_short(char* date) {
	unsigned short bdaysht;
	char dmbuf[3];
	dmbuf[2] = '\0';
	dmbuf[0] = *(date + 5);
	dmbuf[1] = *(date + 6);
	//memcpy(dmbuf, date + 5, 2);
	bdaysht = atoi(dmbuf) * 100;
	//memcpy(dmbuf, date + 8, 2);
	dmbuf[0] = *(date + 8);
	dmbuf[1] = *(date + 9);
	bdaysht += atoi(dmbuf);
	return bdaysht;
}


void* mmapr(char* filename, byteoffset *filelen) {
	int fd;
	struct stat sbuf;
	void *mapaddr;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		fprintf(stderr, "failed to open %s\n", filename);
		exit(1);
    }

    if (stat(filename, &sbuf) == -1) {
        fprintf(stderr, "failed to stat %s\n", filename);
        exit(1);
    }
    
    mapaddr = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapaddr == MAP_FAILED) {
        fprintf(stderr, "failed to mmap %s\n", filename);
        exit(1);
    }
    *filelen = sbuf.st_size;
    return mapaddr;
}

char* makepath(char* dir, char* file, char* ext) {
	char* out = malloc(1024);
	sprintf(out, "%s/%s.%s", dir, file, ext);
	return out;
}


