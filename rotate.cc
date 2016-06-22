// Howto compile: g++ -O2 -o rotate rotate.cc
// (please use compiler optimzation to reduce CPU consumption in production env)
// Usage example: nohup uglyapp | rotate -o ugly.out &
// Author       : peihanw@gmail.com

#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#ifdef __linux__
#include <string.h>
#include <dirent.h>
#endif
#include <string>

#define VERSION_IDENT "@(#)compiled [rotate] at: ["__DATE__"], ["__TIME__"]"
static const char* version_ident = VERSION_IDENT;

static std::string _OutFileNm;
static int _SizeLimit = 100;
static int _SizeLimitBytes = 100 * 1048576;
static int _TimestampFlag = 1;
static int _AppendFlag = 1;
static bool _LongWait = true;
static FILE* _Fp = NULL;
#ifdef __linux__
static bool _PipePidFlag = true;
static pid_t _MyPid = 0;
static char _PipePid[21];
static char _MyPipe[128];
#endif

#define BUFSZ 32768

void _parseArgs(int argc, char* const* argv);
void _usage(const char* exename, int exit_code);
void _now(std::string& now_str, bool with_precision);
FILE* _openOut();
int _selectStdin(bool long_wait);
void _printFp(const std::string& line);
void _consumeStdin(char* buf, std::string& line);
void _mainLoop();
#ifdef __linux__
void _firstLoop(char* buf, std::string& line);
bool _fillMyPipe();
void _scanPipeInfo();
bool _matchPipeInfo(const char* pid);
int _dirFilter(const struct dirent* dir);
int _dirComparator(const struct dirent**, const struct dirent**);
bool _isAllDigits(const char* str);
#endif

int main(int argc, char* const* argv) {
#ifdef __linux__
	memset(_PipePid, '\0', sizeof(_PipePid));
	memset(_MyPipe, '\0', sizeof(_MyPipe));
	_MyPid = getpid();
#endif
	_parseArgs(argc, argv);
	_Fp = _openOut();
	_mainLoop();
}

void _mainLoop() {
	int event_;
	char* buf_ = new char[BUFSZ];
	std::string line_;
#ifdef __linux__
	_firstLoop(buf_, line_);
#endif

	while (true) {
		event_ = _selectStdin(_LongWait);

		if (event_ == 0) {
			if (!_LongWait) {
				_LongWait = true;
				fflush(_Fp);
			}

			continue;
		} else if (event_ < 0) {
			fprintf(stderr, "%s,%d-->ERO: select stdin, errno=%d\n", __FILE__, __LINE__, errno);
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
		fprintf(stderr, "%s,%d-->ERO: read stdin errno=%d\n", __FILE__, __LINE__, errno);
		exit(1);
	} else {
		_LongWait = false;

		for (int i = 0; i < r; ++i) {
			if (_TimestampFlag) {
				if (line.empty()) {
					_now(line, true);
					line.push_back(' ');
#ifdef __linux__

					if (_PipePidFlag) {
						line.append(_PipePid);
					}

#endif
				}
			} else {
#ifdef __linux__

				if (_PipePidFlag && line.empty()) {
					line.append(_PipePid);
				}

#endif
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
		fprintf(stderr, "%s,%d-->ERO: rename(%s,%s), errno=%d\n",
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
		fprintf(stderr, "%s,%d-->ERO: open [%s], errno=%d, use stdout instead\n",
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
#ifdef __linux__

	while ((c = getopt(argc, argv, ":o:s:t:a:p:")) != char(EOF)) {
#else

	while ((c = getopt(argc, argv, ":o:s:t:a:")) != char(EOF)) {
#endif

		switch (c) {
		case ':':
			++err_;
			fprintf(stderr, "%s,%d-->ERO: option -%c needs an argument\n", __FILE__, __LINE__, optopt);
			break;

		case '?':
			++err_;
			fprintf(stderr, "%s,%d-->ERO: unrecognized option -%c\n", __FILE__, __LINE__, optopt);
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
#ifdef __linux__

		case 'p':
			_PipePidFlag = atoi(optarg);
			break;
#endif

		default:
			break;
		}
	}

	if (_OutFileNm.empty()) {
		++err_;
		fprintf(stderr, "%s,%d-->ERO: outFileNm not specified\n", __FILE__, __LINE__);
	}

	if (_SizeLimit <= 9) {
		++err_;
		fprintf(stderr, "%s,%d-->ERO: sizeLimit [%d] should gt 9\n", __FILE__, __LINE__, _SizeLimit);
	}

	if (err_) {
		_usage(argv[0], 1);
	}

	_SizeLimitBytes = _SizeLimit * 1048576;
}

void _usage(const char* exename, int exit_code) {
#ifdef __linux__
	fprintf(stderr, "usage: %s -o outFileNm [-s sizeLimit(MB)] [-t 1|0] [-p 1|0] [-a 1|0]\n", exename);
#else
	fprintf(stderr, "usage: %s -o outFileNm [-s sizeLimit(MB)] [-t 1|0] [-a 1|0]\n", exename);
#endif
	fprintf(stderr, "       -o : output file name\n");
	fprintf(stderr, "       -s : size of file toggle trigger, in MB, default '100'\n");
	fprintf(stderr, "       -t : timestame flag, 1:prepend timestamp, 0:without timestamp, default '1'\n");
#ifdef __linux__
	fprintf(stderr, "       -p : pid flag, 1:prepend pid, 0:no pid, default '1' (pid may incorrect when app forked afterwards)\n");
#endif
	fprintf(stderr, "       -a : append mode, 1:append, 0:trunk, default '1'\n");
	fprintf(stderr, "eg.    %s -o app.out -s 200\n", exename);

	if (exit_code >= 0) {
		exit(exit_code);
	}
}

#ifdef __linux__
void _firstLoop(char* buf, std::string& line) {
	int event_ = _selectStdin(_LongWait);

	if (!_fillMyPipe()) {
		return;
	}

	_scanPipeInfo();

	if (event_ == 0) {
		if (!_LongWait) {
			_LongWait = true;
		}
	} else if (event_ < 0) {
		fprintf(stderr, "%s,%d-->ERO: select stdin, errno=%d\n", __FILE__, __LINE__, errno);
		exit(1);
	} else {
		_consumeStdin(buf, line);
	}
}

bool _fillMyPipe() {
	char path_[1024];
	sprintf(path_, "/proc/%d/fd/0", _MyPid);
	struct stat stat_;
	int r = stat(path_, &stat_);

	if (r != 0) {
		fprintf(stderr, "%s,%d-->WRN: stat(%s), errno=%d\n", __FILE__, __LINE__, path_, errno);
		return false;
	}

	if (!S_ISFIFO(stat_.st_mode)) {
		fprintf(stderr, "%s,%d-->WRN: %s is not a pipe, %d\n", __FILE__, __LINE__, path_, stat_.st_mode);
		return false;
	}

	if (r = readlink(path_, _MyPipe, sizeof(_MyPipe)) < 0) {
		fprintf(stderr, "%s,%d-->WRN: readlink(%s), errno=%d\n", __FILE__, __LINE__, path_, errno);
		return false;
	}

	return true;
}

void _scanPipeInfo() {
	struct dirent** ent_list_ = NULL;
	int r = scandir("/proc", &ent_list_, _dirFilter, _dirComparator);

	if (r < 0) {
		fprintf(stderr, "%s,%d-->WRN: scandir(/proc), errno=%d\n", __FILE__, __LINE__, errno);
		return;
	}

	for (int i = 0; i < r; ++i) {
		if (_matchPipeInfo(ent_list_[i]->d_name)) {
			break;
		}
	}

	for (int i = 0; i < r; ++i) {
		// fprintf(stderr, "%s,%d-->DBG: i=%d, %s\n", __FILE__, __LINE__, i, ent_list_[i]->d_name);
		free(ent_list_[i]);
	}

	free(ent_list_);
}

bool _matchPipeInfo(const char* pid) {
	struct stat stat_;
	struct dirent* dirent_;
	char path_[1024];
	char pipe_[128];
	sprintf(path_, "/proc/%s/fd", pid);
	DIR* dir_ = opendir(path_);

	if (dir_ == NULL) {
		fprintf(stderr, "%s,%d-->WRN: opendir(%s), errno=%d, skip this directory\n", __FILE__, __LINE__, path_, errno);
		return false;
	}

	bool found_ = false;

	while ((dirent_ = readdir(dir_)) != NULL) {
		if (!_isAllDigits(dirent_->d_name)) {
			continue;
		}

		sprintf(path_, "/proc/%s/fd/%s", pid, dirent_->d_name);
		int r = stat(path_, &stat_);

		if (r != 0) {
			fprintf(stderr, "%s,%d-->WRN: stat(%s), errno=%d, skip this fd\n", __FILE__, __LINE__, path_, errno);
			continue;
		}

		if (!S_ISFIFO(stat_.st_mode)) {
			continue;
		}

		if (r = readlink(path_, pipe_, sizeof(pipe_)) < 0) {
			fprintf(stderr, "%s,%d-->WRN: readlink(%s), errno=%d, skip this fd\n", __FILE__, __LINE__, path_, errno);
			continue;
		}

		if (strcmp(_MyPipe, pipe_) == 0) {
			// fprintf(stderr, "%s,%d-->INF: %s %s found\n", __FILE__, __LINE__, path_, pipe_);
			found_ = true;
			sprintf(_PipePid, "%s ", pid);
			break;
		}
	}

	closedir(dir_);
	return found_;
}

int _dirFilter(const struct dirent* dir) {
	if (dir->d_type != DT_DIR) {
		return 0;
	}

	if (!_isAllDigits(dir->d_name)) {
		return 0;
	}

	pid_t pid_ = atoi(dir->d_name);

	if (pid_ == _MyPid) {
		return 0;
	}

	struct stat stat_;

	char path_[1024];

	sprintf(path_, "/proc/%s", dir->d_name);

	if (stat(path_, &stat_) != 0) {
		fprintf(stderr, "%s,%d-->WRN: stat(%s), errno=%d, skip this directory\n", __FILE__, __LINE__, path_, errno);
		return 0;
	}

	if (stat_.st_uid == getuid() || stat_.st_uid == geteuid()) {
		// fprintf(stderr, "%s,%d-->DBG: %s,%d\n", __FILE__, __LINE__, dir->d_name, dir->d_type);
		return 1;
	} else {
		return 0;
	}
}

int _dirComparator(const struct dirent** lhs, const struct dirent** rhs) {
	pid_t pid_lhs_ = atoi((*lhs)->d_name);
	pid_t pid_rhs_ = atoi((*rhs)->d_name);
	int dist_lhs_ = abs(_MyPid - pid_lhs_);
	int dist_rhs_ = abs(_MyPid - pid_rhs_);

	if (dist_lhs_ < dist_rhs_) {
		return -1;
	} else if (dist_lhs_ > dist_rhs_) {
		return 1;
	} else if (pid_lhs_ > _MyPid) {
		return -1;
	} else {
		return 0;
	}
}

bool _isAllDigits(const char* str) {
	const char* p = str;

	if (!*p) {	// regard null str as false
		return false;
	}

	while (*p) {
		if (!isdigit(*p)) {
			return false;
		}

		p++;
	}

	return true;
}
#endif

