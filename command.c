//Note: should translate smoothly to mbed as everything is low-level c.

#include <stdio.h>
#include <stdlib.h>

struct State {
	int rotate;		//1 if R
	int velocity;	//1 if V
	int state;		//different states of DFA (1 = R, 2 = V, 3 = RV)
	float rotVal;
	float velVal;
};

typedef struct State* State_h;

void stateInit(State_h s) {
	s->rotate = 0;
	s->velocity = 0;
	s->state = 0;
	s->rotVal = 0;
	s->velVal = 0;
}

int parseDigit(char c) {
	return (
			c == '0' || c == '1' || c == '2' || c == '3' || c == '4'
			 || c == '5' || c == '6' || c == '7' || c == '8' || c == '9'
			);
}

//should only be called if parseDigit is successful
int readDigit(char c) {
	char* digitArray = "0123456789";
	for (int i=0; i < 10; i++) {
		if (digitArray[i] == c) {
			return i;
		}
	}
}

int parseChar(char c, char input) {
	return (input==c);
}

///returns the index of the last digit of the number and sets result
int parseNumber(char* s, int start, float* result) {
	int foundDecimal = 0;
	int foundMinus = 0;
	int i = start;
	while (i < 16 && (parseDigit(s[i]) || parseChar('.', s[i]) || parseChar('-', s[i]))) {
		if (parseChar('.', s[i]) && (i == start || foundDecimal)) {
			i = 0;
			break;
		}
		else if (parseChar('.', s[i])){
			foundDecimal = 1;
		}
		else if (parseChar('-', s[i]) && (i != start || foundMinus)) {
			i = 0;
			break;
		}
		else if (parseChar('-', s[i])){
			foundMinus = 1;
		}
		i++;
	}
	//if we managed to parse one of more digits
	if (i > start) {
		//take the characters between and including start and i
		char num[17] = "0000000000000000";
		for (int j = start; j < i+1; j++) {
			num[j-start] = s[j];
		}
		*result = atof(num);
		return i;
	}
	else {
		//falied to parse any number
		return -1;
	}
}

int main() {
	while (1) {
		char input[16];
		State_h s = (struct State*)malloc(sizeof(struct State));
		stateInit(s);
		int finished = 0;
		//scan input
		scanf("%s", &input);
		//printf("input = %s\n", input);
		//parse input
		int i = 0;
		while (s->state < 2 && input[i] != '\0') {
			switch (input[i]) {
				case 'R': case 'r':
					if (s->state == 0) {
						float val = 0.0;
						int newPos = parseNumber(input, i+1, &val);
						if (newPos != -1) {
							//printf("rotVal = %f\n", val);
							s->state = 1;
							s->rotate = 1;
							s->rotVal = val;
							i = newPos-1;
						}
					}
					break;
				case 'V': case 'v':
					if (s->state == 0 || s->state == 1) {
						float val = 0.0;
						int newPos = parseNumber(input, i+1, &val);
						if (newPos != -1) {
							//printf("velVal = %f\n", val);
							s->state = 2;
							s->velocity = 1;
							s->velVal = val;
							i = newPos-1;
						}
					}
			}

			i++;
		}
		//call threads here
		if (s->rotate && s->velocity) {
			printf("Calling rotate and velocity function with R=%f, V=%f...\n", s->rotVal, s->velVal);
			//insert here
		}
		else if (s->rotate) {
			printf("Calling just rotate function with R=%f...\n", s->rotVal);
		}
		else if (s->velocity) {
			printf("Calling just velocity function with V=%f...\n", s->velVal);
		}
		free(s);
	}

	return 0;
}