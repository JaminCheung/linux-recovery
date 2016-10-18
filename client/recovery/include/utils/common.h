#ifndef COMMON_H_
#define COMMON_H_
#ifdef __cplusplus
extern "C" {
#endif

#define _new(T, P)                          \
({                                          \
    T * obj = (T *)calloc(1, sizeof(T));    \
    obj->construct = construct_##P;         \
    obj->destruct = destruct_##P;           \
    obj->construct(obj);                    \
    obj;                                    \
})

#define _delete(P)                          \
({                                          \
    P->destruct(P);                         \
    free((void *)(P));                      \
})

int get_multiplier(const char *str);
long long get_bytes(const char *str);
void print_bytes(long long bytes, int bracket);

#ifdef __cplusplus
}
#endif
#endif /* COMMON_H_ */
