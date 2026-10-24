#ifndef LIBPATH_H
#define LIBPATH_H

extern const char *path_basename(const char *path);
extern char *path_dirname(const char *path);
extern const char *path_extension(const char *path);
extern char *path_stem(const char *path);
extern char *path_set_extension(const char *path,const char *ext);
extern char *path_join(const char *base,const char *sub);
extern char *path_normalize(const char *path);
extern int path_is_descendant_of(const char *path, const char *base);
extern char *path_relative_to(const char *from, const char *to);
extern char *path_to_absolute(const char *path);
extern char *path_canonicalize(const char *path);
#endif
