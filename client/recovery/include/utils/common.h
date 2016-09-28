#ifndef COMMON_H_
#define COMMON_H_
#ifdef __cplusplus
extern "C" {
#endif

int get_multiplier(const char *str);
long long get_bytes(const char *str);
void print_bytes(long long bytes, int bracket);

#ifdef __cplusplus
}
#endif
#endif /* COMMON_H_ */
