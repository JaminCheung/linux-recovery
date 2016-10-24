#ifndef JFFS2_H
#define JFFS2_H

int jffs2_init_cleanmaker(struct filesystem *fs, 
                         struct jffs2_unknown_node *maker,
                         int *pos, int *len);
int jffs2_write_cleanmaker(struct filesystem *fs,
                           long long offset,
                           struct jffs2_unknown_node *cleanmarker,
                           int clmpos, int clmlen);
#endif