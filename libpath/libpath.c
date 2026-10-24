#include "libpath.h"
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define getcwd _getcwd
#define os_realpath(p) _fullpath(NULL, (p), 0);
#else
#include <unistd.h>
#define os_realpath(p) realpath((p), NULL);
#endif

static int is_sep(char c){
  if(c == '/' || c == '\\'){
    return 1;
  }
  return 0;
}

extern const char *path_basename(const char *path){
  if(path == NULL) return "";
  int i = 0;
  while(path[i]){
    i++;
  }

  if(i == 0){
    return "";
  }
  if(i == 1){
    if(path[0] == '/'){
      return "/";
    }
    if(path[0] == '\\'){
      return "\\";
    }
    return path;
  }

  i--;
  while(i > 0 && is_sep(path[i])){
    i--;
  }

  if(i == 0){
    if(path[i] == '/'){
      return "/";
    }
    if(path[i] == '\\'){
      return "\\";
    }
  }
  while(i > 0 && !is_sep(path[i])){
    i--;
  }

  if(i == 0){
    if(is_sep(path[i])){
      return &path[i + 1];
    }else{
      return path;
    }
  }

  return &path[i + 1];
}

extern char *path_dirname(const char *path){

  if(path == NULL){
    char *dirname = malloc(2 * sizeof(char));
    if (dirname == NULL) return NULL;
    dirname[0] = '.';
    dirname[1] = '\0';
    return dirname;
  }

  int i = 0;

  while(path[i]){
    i++;
  }
  i--;

  while(i > 0 && is_sep(path[i])){
    i--;
  }

  if(i == 0){
    char *dirname = malloc(2 * sizeof(char));
    if (dirname == NULL) return NULL;
    if(path[i] == '/'){
      dirname[0] = '/';
    }else if(path[i] == '\\'){
      dirname[0] = '\\';
    }else{
      dirname[0] = '.';
    }
    dirname[1] = '\0';
    return dirname;
  }

  while(i >= 0 && !is_sep(path[i])){
    i--;
  }

  while(i > 0){
    if(path[i] == '/' && path[i - 1] == '/'){
      i--;
    }else if(path[i] == '\\' && path[i - 1] == '\\'){
      i--;
    }else{
      break;
    }
  }
  if(i == 0){
    char *dirname = malloc(2 * sizeof(char));
    if (dirname == NULL) return NULL;
    if(path[i] == '/'){
      dirname[0] = '/';
    }else if(path[i] == '\\'){
      dirname[0] = '\\';
    }
    dirname[1] = '\0';
    return dirname;
  }

  if(i < 0){
    char *dirname = malloc(2 * sizeof(char));
    if (dirname == NULL) return NULL;
    dirname[0] = '.';
    dirname[1] = '\0';
    return dirname;
  }


  char *dirname = malloc((i + 2) * sizeof(char));
  if (dirname == NULL) return NULL;
  for(int j = 0; j < i; j++){
    dirname[j] = path[j];
  }
  dirname[i] = '\0';


  return dirname;

}

extern const char *path_extension(const char *path){
  const char *basename = path_basename(path);
  if (basename[0] == '\0') return "";
  
  int i = 0;
  while(basename[i]){
    i++;
  }
  i--;

  while(i > 0 && basename[i] != '.'){
    i--;
  }

  if(i == 0){
    return "";
  }

  return &basename[i + 1];
}

extern char *path_stem(const char *path){
  const char *basename = path_basename(path);
  if (basename[0] == '\0' || is_sep(basename[0])) {
    char *stem = malloc(sizeof(char));
    if (stem == NULL) return NULL;
    stem[0] = '\0';
    return stem;
  }

  int i = 0;
  while(basename[i]){
    i++;
  }
  i--;

  while(i > 0 && basename[i] != '.'){
    i--;
  }
  
  if(i == 0){
    while(basename[i]){
      i++;
    }
  }
  char *stem = malloc((i + 2) * sizeof(char));
  if (stem == NULL) return NULL;
  for(int j = 0; j < i; j++){
    stem[j] = basename[j];
  }
  stem[i] = '\0';
  return stem;

}

extern char *path_set_extension(const char *path,const char *ext){

  if(path == NULL || path[0] == '\0'){
    char *new_ext = malloc(sizeof(char));
    if (new_ext == NULL) return NULL;
    new_ext[0] = '\0';
    return new_ext;
  }
  int ext_null = 0;
  if(ext == NULL || ext[0] == '\0'){
    ext_null = 1;
  }

  int i = 0;
  while(path[i]){
    i++;
  }
  i--;

  while(i > 0 && path[i] != '.'){
    if(is_sep(path[i])){
      i = 0;
      break;
    }
    if(is_sep(path[i-1])){
      i = 0;
      break;
    }
    i--;
  }
  if(i > 0 && is_sep(path[i-1])){
    i = 0;
  }
  
  if(i == 0){
    while(path[i]){
      i++;
    }
  }

  int j = 0;
  if(ext_null == 0){
    while(ext[j]){
      j++;
    }
  }

  char *new_ext = malloc((i+j+2) * sizeof(char));
  if (new_ext == NULL) return NULL;


  for(int k = 0; k < i; k++){
    new_ext[k] = path[k];
  }
  if(ext_null == 1){
    new_ext[i] = '\0';
    return new_ext;
  }
  int has_point = ext[0] == '.' ? 1 : 0;
  if(has_point == 0){
    new_ext[i] = '.';
    i++;
  }
  for(int k = 0; k < j; k++){
    new_ext[i+k] = ext[k];
  }
  new_ext[i+j] = '\0';

  return new_ext;
}

static char *duplicate_str(const char *s){
  int i = 0;
  while(s[i]){
    i++;
  }
  char *str = malloc((i+1)*sizeof(char));
  if (str == NULL) return NULL;

  for(int j = 0; j < i; j++){
    str[j] = s[j]; 
  }
  str[i] = '\0';
  return str;
}
extern char *path_join(const char *base,const char *sub){
  if((base == NULL || base[0] == '\0') && (sub == NULL || sub[0] == '\0')){
    char *join = malloc(sizeof(char));
    if (join == NULL) return NULL;

    join[0] = '\0';
    return join;
  }
  if(base == NULL || base[0] == '\0'){
    return duplicate_str(sub);
  }
  if(sub == NULL || sub[0] == '\0'){
    return duplicate_str(base);
  }
  
  if(
      is_sep(sub[0]) || 
      // sub[0] == '/' || //path assoluto unix
      // (sub[0] == '\\') || //path rete UNC
      (((sub[0] >= 'A' && sub[0] <='Z') || (sub[0] >= 'a' && sub[0] <='z')) && (sub[1] != '\0' && sub[1] == ':') && (sub[2] != '\0' && is_sep(sub[2]))) //path windows
      ){
    return duplicate_str(sub);
  }


  int i = 0;
  int has_forward_slash = 1;
  while(base[i]){
    if(base[i] == '\\'){
      has_forward_slash = 0;
    }
    i++;

  }
  i--;

  int j = 0;
  while(sub[j]){
    j++;
  }
  j--;

  while(i >= 0 && is_sep(base[i])){
    i--;
  }
  while(j >= 0 && is_sep(sub[j])){
    j--;
  }

  char *join = malloc((i+j+4) * sizeof(char));
  if (join == NULL) return NULL;
  for(int k = 0; k <= i; k++){
    join[k] = base[k];
  }
  if(has_forward_slash == 1){
    join[i+1] = '/';
  }else{
    join[i+1] = '\\';
  }
  for(int k = 0; k <= j; k++){
    join[i+k+2] = sub[k];
  }
  join[i+j+3] = '\0';
  return join;
}

extern char *path_normalize(const char *path){
  if(path == NULL){
    char *normal = malloc(sizeof(char));
    if (normal == NULL) return NULL;
    normal[0] = '\0';
    return normal;
  }
  if(path[0] == '\0'){
    char *normal = malloc(2 * sizeof(char));
    if (normal == NULL) return NULL;

    normal[0] = '.';
    normal[1] = '\0';
    return normal;
  }

  int i = 0;
  char sep = '/';
  while(path[i]){
    if(path[i] == '/'){
      sep = '/';
    }else if(path[i] == '\\'){
      sep = '\\';
    }
    i++;
  }
  i--;


  int out = 0;
  char *normal = malloc((i+2) * sizeof(char));
  if (normal == NULL) return NULL;


  i = 0;
  int start = 0;
  int is_abs = is_sep(path[0]);
  if(is_sep(path[0])){
    normal[out] = sep;
    out++;
  }

  while(path[i]){
    while(path[i] && is_sep(path[i])){
      i++;
    }
    start = i;
    while(path[i] && !is_sep(path[i])){
      i++;
    }
    int len = i - start;

    if((len == 1 && path[start] == '.') || len == 0){
      continue;
    }else if(len == 2 && path[start] == '.' && path[start + 1] == '.'){
      int last_is_dots = (out >= 2 && normal[out - 1] == '.' && normal[out - 2] == '.' &&
          (out == 2 || is_sep(normal[out - 3])));

      int is_root = (out == 1 && is_sep(normal[0])) ||
        (out == 2 && normal[1] == ':');

      if(out > 0 && !is_root && !last_is_dots){
        out--; // arretra dall'ultimo carattere
        while(out > 0 && !is_sep(normal[out - 1])){
          out--;
        }
        if(out > 0 && !(out == 1 && is_sep(normal[0])) && normal[out - 1] != ':'){
          out--;
        }
      }else if(!is_abs && !is_root){
        if(out > 0 && !is_sep(normal[out - 1])){
          normal[out] = sep;
          out++;
        }
        normal[out] = '.';
        out++;
        normal[out] = '.';
        out++;
      }
    }else{
      if(out > 0 && !is_sep(normal[out - 1])){
        normal[out] = sep;
        out++;
      }
      for(int j = start; j < i; j++){
        normal[out] = path[j];
        out++;
      }
    }
  }

  if(out == 0){
    normal[0] = is_abs ? sep : '.';
    normal[1] = '\0';
  }else{
    normal[out] = '\0';
  }




  return normal;
}
extern int path_is_descendant_of(const char *path, const char *base){
  
  if (base == NULL || base[0] == '\0' || path == NULL || path[0] == '\0') {
    return 0;
  }

  char *path_normal = path_normalize(path);
  char *base_normal = path_normalize(base);
  if(path_normal == NULL || base_normal == NULL){
    return 0;
  }

  int len_path = 0;
  while(path_normal[len_path]){
    len_path++;
  }
  int len_base = 0;
  while(base_normal[len_base]){
    len_base++;
  }

  if(len_path > 0 && len_base > 0){
    if(is_sep(path_normal[0]) != is_sep(base_normal[0])){
      free(path_normal);
      free(base_normal);
      return 0;
    }
  }



  int i = 0;
  while(i < len_base && i < len_path && base_normal[i] && path_normal[i]){
    if(base_normal[i] != path_normal[i]){
      free(path_normal);
      free(base_normal);
      return 0;
    }
    i++;
  }
  if(i == len_base && i == len_path){
    free(path_normal);
    free(base_normal);
    return 1;
  }
  if(i == 1 && len_base == 1 && is_sep(base_normal[0])){
    free(path_normal);
    free(base_normal);
    return 1; 
  }
  if(is_sep(path_normal[i])){
    free(path_normal);
    free(base_normal);
    return 1;
  }
  
  free(path_normal);
  free(base_normal);
  return 0;
}

static char char_to_lower(char c){
  if(c >= 'a' && c <= 'z') return c;
  return c + 32;
}
extern char *path_relative_to(const char *from, const char *to){

  if (from == NULL || from[0] == '\0' || to == NULL || to[0] == '\0') {
    return NULL;
  }



  char *from_normal = path_normalize(from);
  char *to_normal = path_normalize(to);

  if(from_normal == NULL || to_normal == NULL){
    return 0;
  }

  int len_from_normal = 0;
  while(from_normal[len_from_normal]){
    len_from_normal++;
  }
  int len_to_normal = 0;
  while(to_normal[len_to_normal]){
    len_to_normal++;
  }

  if(len_from_normal > 0 && len_to_normal > 0){
    if(is_sep(from_normal[0]) != is_sep(to_normal[0])){
      free(from_normal);
      free(to_normal);
      return NULL;
    }
  }
  
  int has_drive_from = (len_from_normal >= 2 && from_normal[1] == ':') ? from_normal[0] : -1;
  int has_drive_to = (len_to_normal >= 2 && to_normal[1] == ':') ? to_normal[0] : -1;
  if(has_drive_from != -1 && has_drive_to != -1 && char_to_lower(has_drive_from) != char_to_lower(has_drive_to)){
    free(from_normal);
    free(to_normal);
    return NULL;
  }
  if((has_drive_from == -1) != (has_drive_to == -1)){
    free(from_normal);
    free(to_normal);
    return NULL;
  }


  char *result = malloc((len_from_normal * 3 + len_to_normal + 2) * sizeof(char));
  if(result == NULL){
    free(from_normal);
    free(to_normal);
    return NULL;
  }
  int i = 0;
  int j = 0;
  int start_from = 0;
  int start_to = 0;

  while(from_normal[i] && to_normal[j]){
    while(from_normal[i] && is_sep(from_normal[i])){
      i++;
    }
    start_from = i;
    while(from_normal[i] && !is_sep(from_normal[i])){
      i++;
    }
    int len_from = i - start_from;

    while(to_normal[j] && is_sep(to_normal[j])){
      j++;
    }
    start_to = j;
    while(to_normal[j] && !is_sep(to_normal[j])){
      j++;
    }
    int len_to = j - start_to;
    

    if(len_from != len_to){
      i = start_from;
      j = start_to;
      break;
    }

    int flag = 0;
    for(int k = 0; k < len_from; k++){
      if(from_normal[start_from + k] != to_normal[start_to + k]){
        flag = 1;
        i = start_from;
        j = start_to;
        break;
      }
    }
    if(flag == 1) break;
  }

  if(i == len_from_normal && j == len_to_normal){
    result[0] = '.';
    result[1] = '\0';
    free(from_normal);
    free(to_normal);
    return result;
  }

  int folder_in_to = 0;
  while(from_normal[i]){
    while(from_normal[i] && is_sep(from_normal[i])){
      i++;
    }
    start_from = i;
    while(from_normal[i] && !is_sep(from_normal[i])){
      i++;
    }
    int len_from = i - start_from;
    if(len_from > 0){
      folder_in_to++;
    }
  }

  int out = 0;
  i = 0;

  for(i = 0; i < folder_in_to; i++){
    result[out] = '.';
    result[out+1] = '.';
    if(i != folder_in_to - 1){
      result[out+2] = '/';
      out+=3;
    }else{
      out+=2;
    }
  }

  //salto lo slash dove j è
  while(is_sep(to_normal[j])){
    j++;
  }

  if(j < len_to_normal){
    if(out > 0){
      result[out] = '/';
      out++;
    }
    for(i = j; i < len_to_normal; i++){
      result[out] = to_normal[i];
      out++;
    }
  }
  result[out] = '\0';

  free(from_normal);
  free(to_normal);

  return result;
}


extern char *path_to_absolute(const char *path){
  
  if(path == NULL){
    return NULL;
  }

  if(is_sep(path[0]) || (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) && path[1] && path[1] == ':' && path[2] && is_sep(path[2]))){
    return path_normalize(path);
  }

  char *cwd = malloc(4096 * sizeof(char));
  if(cwd == NULL){
    return NULL;
  }

  cwd = getcwd(cwd, 4096);
  if(cwd == NULL){
    return NULL;
  }

  char *result = NULL;
  char *join = path_join(cwd, path);
  result = path_normalize(join);



  free(cwd);
  free(join);
  return result;
}

extern char *path_canonicalize(const char *path){

  if(path == NULL || path[0] == '\0'){
    return NULL;
  }

  char *result = os_realpath(path);
  if(result == NULL){
    return NULL;
  }
  return result;

}
