//
//  main.cpp
//  ezip
//
//  Created by town42 on 2020/04/09.
//  Copyright © 2020 ci-sales. All rights reserved.
//

#include <iostream>
#include <random>
#include <dirent.h>
#include <sys/stat.h>
#include <zip.h>
#include <iconv.h>

#define BUFSIZE 1024
#define ENC_SRC "UTF-8"
#define ENC_DST "cp932"
using namespace std;

/*
std::string replace_all(std::string input, std::string from,
                       std::string to) {
    unsigned long pos = input.find(from);
    long toLen = to.length();
 
    if (from.empty()) {
        return input;
    }
 
    while ((pos = input.find(from, pos)) != std::string::npos) {
        input.replace(pos, from.length(), to);
        pos += toLen;
    }
    return input;
}

std::string convert_to_sjis(const char* text)
{
    iconv_t cd;
    size_t srclen, destlen;
    size_t ret;
    
    cd = iconv_open(ENC_DST, ENC_SRC);
    if (cd == (iconv_t)-1) {
        throw std::runtime_error("Failed to open iconv");
        return 0;
    }

    srclen = strlen(text);
    char input[srclen + 1];
    snprintf(input, sizeof(input), "%s", text);
    char* inbuf = input;
    printf("%s: [%d]", inbuf, (int)srclen);
    
    destlen = BUFSIZE;
    char output[destlen + 1];
    memset(output, '\0', destlen);
    char* outbuf = output;
    
    ret = iconv(cd, &inbuf, &srclen, &outbuf, &destlen);
    if (ret == -1) {
        throw std::runtime_error("iconv returns invalid output");
        return 0;
    }

    iconv_close(cd);

    return string(output);
}
*/

string random_text(int length) {
    string text;
 
    generate_n(text.begin(), length, []() -> char {
        random_device rnd;
        mt19937 mt(rnd());
        const char char_set[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        char result = char_set[mt() % (sizeof(char_set) - 1)];
        return result;
    });
 
    return text;
}

bool file_exists(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return (st.st_mode & S_IFMT) == S_IFREG;
}

bool directory_exists(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

string get_filename(const char* path)
{
    std::string str_path = std::string(path);
    std::string::size_type separate_pos = str_path.find_last_of("/");
    if(separate_pos == std::string::npos){
        return str_path;
    }
    return str_path.substr(separate_pos + 1);
}

string trim_file_extension(const char* file_name)
{
    std::string str_file_name = std::string(file_name);
    std::string::size_type separate_pos = str_file_name.find_last_of(".");
    if(separate_pos == std::string::npos){
        return file_name;
    }
    return str_file_name.substr(0, separate_pos);
}

void _zip_single_file(zip_t *zipper, const char* src_path, const char* dest_path){
    zip_source_t *source = zip_source_file(zipper, src_path, 0, 0);
    if (source == nullptr) {
      throw std::runtime_error("Failed to add file to zip: " + std::string(zip_strerror(zipper)));
    }
    
    //const char *dest_path_encoded = convert_to_sjis(dest_path).c_str();
    if (zip_file_add(zipper, dest_path, source, ZIP_FL_OVERWRITE) < 0) {
      zip_source_free(source);
      throw std::runtime_error("Failed to add file to zip: " + std::string(zip_strerror(zipper)));
    } else {
        printf("Add file: %s\n", dest_path);
    }
}

void _zip_directory_delegate(const char *name, const char *parent,
                             const char *foldername, zip_t *zipper)
{
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name))){
      if(file_exists(name)){
          const char* output_filename = get_filename(name).c_str();
          char output_path[BUFSIZE];
          snprintf(output_path, sizeof(output_path), "%s/%s",
                              foldername, output_filename);
          _zip_single_file(zipper, name, output_path);
      }
      return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0
            || strcmp(entry->d_name, ".DS_Store") == 0)
            continue;
        
        char entry_path[BUFSIZE];
        if(strcmp(parent, "") == 0){
            snprintf(entry_path, sizeof(entry_path), "%s", entry->d_name);
        } else {
            snprintf(entry_path, sizeof(entry_path), "%s/%s", parent, entry->d_name);
        }
        
        if (entry->d_type == DT_DIR) {
            char path[BUFSIZE];
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            _zip_directory_delegate(path, entry_path, foldername, zipper);
        } else {
            char full_path[BUFSIZE];
            char output_path[BUFSIZE];
            snprintf(full_path, sizeof(full_path), "%s/%s", name, entry->d_name);
            snprintf(output_path, sizeof(output_path), "%s/%s",
                                foldername, entry_path);
            _zip_single_file(zipper, full_path, output_path);
        }
    }
    closedir(dir);
}


void _zip_directory(const std::string& inputdir,
                          const std::string& output_filename,
                          const std::string& foldername)
{
  int errorp;
  zip_t *zipper = zip_open(output_filename.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorp);
  if (zipper == nullptr) {
    zip_error_t ziperror;
    zip_error_init_with_code(&ziperror, errorp);
    throw std::runtime_error("Failed to open output file " + output_filename + ": " + zip_error_strerror(&ziperror));
  }

  try {
      _zip_directory_delegate(inputdir.c_str(), "", foldername.c_str(), zipper);
  } catch(...) {
    zip_close(zipper);
    throw;
  }
  
  string password = random_text(8);
  zip_int64_t num = zip_get_num_entries(zipper, 0);
  for (zip_int64_t i = 0; i < num; i++){
    // ZIP_EM_TRAD_PKWARE Encryption DID NOT WORK, wait for libzip 1.7.0
    zip_file_set_encryption(zipper, i, ZIP_EM_AES_256, password.c_str());
  }
  printf("password: %s\n", password.c_str());
  
  zip_close(zipper);
}

int main(int argc, const char * argv[]) {
    if(argc > 1)
    {
        const char * file_path=argv[1];
        if (file_exists(file_path) || directory_exists(file_path))
        {
            string output_name = trim_file_extension(get_filename(file_path).c_str());
            _zip_directory(string(file_path),
                           "./" + output_name + ".zip", output_name);
        }
        else
        {
          throw std::runtime_error("File not found: " + string(file_path));
        }
    }
    return 1;
}
