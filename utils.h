#define REPORTING_N 1000000
#define LINEBUFLEN 1024

typedef unsigned long byteoffset;
typedef unsigned int  entrycount;

struct Person {
	long person_id;
	long linenum;
	short birthday;
	long location;
	byteoffset knows_first;
	entrycount knows_n;
	byteoffset interests_first;
	entrycount interest_n;
};

struct Result { 
    long person_id;
    long knows_id;
    short score;
};

void parse_csv(char* fname, void (*field_handler)(int, char*), void  (*line_finisher)()) {
	long nlines = 0;

   	FILE* stream = fopen(fname, "r");
	if (stream == NULL) {
		fprintf(stderr, "Can't read file at %s\n", fname);
		exit(-1);
	}
	char line[LINEBUFLEN];
	while (fgets(line, LINEBUFLEN, stream)) {
	 	char* tok;
	    int col = 0;
		for (tok = strtok(line, "|");
		        tok && *tok;
		        tok = strtok(NULL, "|\n")) {
			(*field_handler)(col, tok);
			col++;
		}
		(*line_finisher)();
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
short birthday_to_short(char* date) {
	short bdaysht;
	char dmbuf[3];
	dmbuf[2] = '\0';
	memcpy(dmbuf, date + 5, 2);
	bdaysht = atoi(dmbuf) * 100;
	memcpy(dmbuf, date + 8, 2);
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


