// Howto compile: g++ -O2 -o rotate rotate.cc
// (please use compiler optimzation to reduce CPU consumption in production env)
// Usage example: nohup uglyapp | rotate -o ugly.out -t 1 &
// Author       : peihanw@gmail.com

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>

#define VERSION_IDENT "@(#)compiled [rotate] at: ["__DATE__"], ["__TIME__"]"
static const char* version_ident = VERSION_IDENT;

static std::string _OutFileNm;
static int _SizeLimit = 100;
static int _SizeLimitBytes = 100 * 1048576;
static int _TimestampFlag = 0;
static int _AppendFlag = 1;
static bool _LongWait = true;
static FILE* _Fp = NULL;

#define BUFSZ 32768

void _parseArgs(int argc, char* const* argv);
void _usage(const char* exename, int exit_code);
void _now(std::string& now_str, bool with_precision);
FILE* _openOut();
int _selectStdin(bool long_wait);
void _printFp(const std::string& line);
void _consumeStdin(char* buf, std::string& line);
void _mainLoop();

int main(int argc, char* const* argv) {
	_parseArgs(argc, argv);
	_Fp = _openOut();
	_mainLoop();
}

void _mainLoop() {
	int event_;
	char* buf_ = new char[BUFSZ];
	std::string line_;

	while (true) {
		event_ = _selectStdin(_LongWait);

		if (event_ == 0) {
			if (!_LongWait) {
				_LongWait = true;
				fflush(_Fp);
			}

			continue;
		} else if (event_ < 0) {
			fprintf(stderr, "@@ %s,%d-->Error: select stdin, errno=%d\n", __FILE__, __LINE__, errno);
			exit(1);
		} else {
			_consumeStdin(buf_, line_);
		}
	}
}

void _consumeStdin(char* buf, std::string& line) {
	ssize_t r = read(fileno(stdin), buf, BUFSZ);

	if (r == 0) { // EOF
		exit(0);
	} else if (r < 0) {
		fprintf(stderr, "@@ %s,%d-->Error: read stdin errno=%d\n", __FILE__, __LINE__, errno);
		exit(1);
	} else {
		_LongWait = false;

		for (int i = 0; i < r; ++i) {
			if (_TimestampFlag && line.empty()) {
				_now(line, true);
				line.push_back(' ');
			}

			line.push_back(buf[i]);

			if (buf[i] == '\n') {
				_printFp(line);
				line = "";
			}
		}
	}
}

void _printFp(const std::string& line) {
	fprintf(_Fp, "%s", line.data());

	if (fileno(_Fp) == 1) {
		return;
	}

	long pos_ = ftell(_Fp);

	if (pos_ < _SizeLimitBytes) {
		return;
	}

	fprintf(_Fp, "*** file size %ld reach rotate limit ***\n", pos_);
	fclose(_Fp);
	std::string now_str_;
	_now(now_str_, false);
	std::string bak_nm_(_OutFileNm);
	bak_nm_.append(".");
	bak_nm_.append(now_str_);

	if (rename(_OutFileNm.data(), bak_nm_.data()) != 0) {
		fprintf(stderr, "@@ %s,%d-->Error: rename(%s,%s), errno=%d\n",
			__FILE__, __LINE__, _OutFileNm.data(), bak_nm_.data(), errno);
		_Fp = stdout;
	} else {
		_Fp = _openOut();
	}
}

int _selectStdin(bool long_wait) {
	int r;
	int nfds_ = fileno(stdin) + 1;
	fd_set readfds_;
	struct timeval timeout_;

	while (true) {
		if (long_wait) {
			timeout_.tv_sec = 10;
			timeout_.tv_usec = 0;
		} else {
			timeout_.tv_sec = 0;
			timeout_.tv_usec = 50000;
		}

		FD_ZERO(&readfds_);
		FD_SET(fileno(stdin), &readfds_);
		r = select(nfds_, &readfds_, NULL, NULL, &timeout_);

		if (r < 0 && errno == EINTR) {
			continue;
		}

		return r;
	}
}

FILE* _openOut() {
	FILE* fp_ = NULL;

	if (_AppendFlag) {
		fp_ = fopen(_OutFileNm.data(), "a+");
	} else {
		fp_ = fopen(_OutFileNm.data(), "w");
	}

	if (fp_ == NULL) {
		fprintf(stderr, "@@ %s,%d-->Fatal: open [%s], errno=%d, use stdout instead\n",
			__FILE__, __LINE__, _OutFileNm.data(), errno);
		return stdout;
	}

	return fp_;
}

void _now(std::string& now_str, bool with_precision) {
	char buf_[64];
	struct timeval tv_;
	::gettimeofday(&tv_, NULL);
	struct tm tm_;
	::localtime_r(&tv_.tv_sec, &tm_);
	strftime(buf_, sizeof (buf_), "%Y%m%d%H%M%S", &tm_);
	now_str = buf_;

	if (!with_precision) {
		return;
	}

	unsigned msec_ = tv_.tv_usec % 1000000;
	sprintf(buf_, ".%06d", msec_);
	now_str.append(buf_);
}

void _parseArgs(int argc, char* const* argv) {
	int err_ = 0;
	char c;

	while ((c = getopt(argc, argv, ":o:s:t:a:")) != char(EOF)) {
		switch (c) {
		case ':':
			++err_;
			fprintf(stderr, "Fatal: option -%c needs an argument\n", optopt);
			break;

		case '?':
			++err_;
			fprintf(stderr, "Fatal: unrecognized option -%c\n", optopt);
			break;

		case 'o':
			_OutFileNm = optarg;
			break;

		case 's':
			_SizeLimit = atoi(optarg);
			break;

		case 't':
			_TimestampFlag = atoi(optarg);
			break;

		case 'a':
			_AppendFlag = atoi(optarg);
			break;

		default:
			break;
		}
	}

	if (_OutFileNm.empty()) {
		++err_;
		fprintf(stderr, "Fatal: outFileNm not specified\n");
	}

	if (_SizeLimit <= 9) {
		++err_;
		fprintf(stderr, "Fatal: sizeLimit [%d] should gt 9\n", _SizeLimit);
	}

	if (err_) {
		_usage(argv[0], 1);
	}

	_SizeLimitBytes = _SizeLimit * 1048576;
}

void _usage(const char* exename, int exit_code) {
	fprintf(stderr, "usage: %s -o outFileNm [-s sizeLimit(MB)] [-t 0|1] [-a 1|0]\n", exename);
	fprintf(stderr, "       -o : output file name\n");
	fprintf(stderr, "       -s : size of file toggle trigger, in MB, default '100'\n");
	fprintf(stderr, "       -t : timestame flag, 0:without timestamp, 1:prepend timestamp, default '0'\n");
	fprintf(stderr, "       -a : append mode, 1:append, 0:trunk, default '1'\n");
	fprintf(stderr, "eg.    %s -o app.out -s 200 -t 1\n", exename);

	if (exit_code >= 0) {
		exit(exit_code);
	}
}

