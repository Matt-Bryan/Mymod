#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#define MAX_FILENAME 15
#define MAX_MODE 10
#define NUM_FILES 5

#define CHANGES 0b100000
#define NO_PRES 0b010000
#define PRES 0b001000
#define SILENT 0b000100
#define VERBOSE 0b000010
#define RECURSE 0b000001

typedef struct Info {
   char **files;
   char mode[MAX_MODE];
}   Info;

Info *ProcessArgv(int argc, char **argv, unsigned char *ops) {
   int count, fileCount = 0, modeChanged = 0;
   char *newFile, *curArg;
   struct stat fileStat;
   Info *info = calloc(1, sizeof(Info));
   info->files = calloc(NUM_FILES, sizeof(char **));

   for (count = 1; count < argc; count++) {
      if (argv[count][0] == '-') {
         curArg = argv[count];
         if (strcmp("-c", curArg) == 0 || strcmp("--changes", curArg) == 0) {
            *ops |= CHANGES;
            *ops &= (~VERBOSE);
         }
         else if (strcmp("--no-preserve-root", curArg) == 0) {
            //don't treat / specially
            *ops |= NO_PRES;
            *ops &= (~PRES);
         }
         else if (strncmp("--reference=", curArg, 12) == 0) {
            if (stat(&(curArg[12]), &fileStat) < 0) {
               fprintf(stderr, "BAD STAT\n");
               exit(-1);
            }
            sprintf(info->mode, "%o", fileStat.st_mode);
            sprintf(info->mode, "%s", &(info->mode[2]));
            //printf("info->mode: %s\n", info->mode);
         }
         else if (strcmp("--preserve-root", curArg) == 0) {
            //fail to operate recursively on /
            *ops |= PRES;
            *ops &= (~NO_PRES);
         }
         else if (strcmp("-f", curArg) == 0 || strcmp("--silent", curArg) == 0
          || strcmp("--quiet", curArg) == 0) {
            //suppress most error messages
            *ops |= SILENT;
         }
         else if (strcmp("-v", curArg) == 0 || strcmp("--verbose", curArg) == 0) {
            //output a diagnostic for every file processed
            *ops |= VERBOSE;
            *ops &= (~CHANGES);
         }
         else if (strcmp("-R", curArg) == 0 || strcmp("--recursive", curArg) == 0) {
            //change files and directories recursively
            *ops |= RECURSE;
         }
         else if (strcmp("--help", curArg) == 0) {
            //print help message and quit
            printf("Usage: mymod [MODE] [Filename]\n");
            exit(0);
         }
         else if (strcmp("--version", curArg) == 0) {
            //output version info and exit
            printf("mymod - Matt Bryan\nThis is my attempt to copy");
            printf(" the chmod function\n");
            exit(0);
         }
         else {
            printf("mymod: unrecognized argument %s\n", curArg);
            exit(-1);
         }
      }
      else if (access(argv[count], F_OK) != 0 && modeChanged == 0 && atoi(argv[count]) >= 0) {
         strcpy(info->mode, argv[count]);
         modeChanged = 1;
         //printf("new mode: %s\n", info->mode);
      }
      else {
         newFile = calloc(MAX_FILENAME, sizeof(char));
         strcpy(newFile, argv[count]);
         info->files[fileCount++] = newFile;
      } 
   }
   return info;
}

int ChangeMode(char *fileName, unsigned char ops, Info *info) {
   char newMode[5], testMode[5];
   int error, modeCount = 0, oldStderr, changedMode = 0;
   struct stat fileStat;

   if (ops & SILENT) {
      oldStderr = dup(2);
      freopen("/dev/null", "w", stderr);
      fclose(stderr);
   }

   if (access(fileName, F_OK) != 0) {
      fprintf(stderr, "Cannot access file '%s'\n", fileName);
      exit(-1);
   }
   stat(fileName, &fileStat);
   if (S_ISDIR(fileStat.st_mode)) {
      sprintf(testMode, "%o", fileStat.st_mode);
      sprintf(testMode, "%s", &(testMode[2]));
      changedMode = (atoi(info->mode) == atoi(testMode)) ? 0 : 1;
      error = chmod(fileName, strtoul(info->mode, NULL, 8));
   }
   else {
      if (strlen(info->mode) == 4) {
         sprintf(testMode, "%o", fileStat.st_mode);
         sprintf(testMode, "%s", &(testMode[2]));
         changedMode = (atoi(&(info->mode[1])) == atoi(testMode)) ? 0 : 1;
         error = chmod(fileName, strtoul(&(info->mode[1]), NULL, 8));
      }
      else {
         sprintf(testMode, "%o", fileStat.st_mode);
         sprintf(testMode, "%s", &(testMode[2]));
         newMode[modeCount++] = '0';
         while (modeCount < 4) {
            newMode[modeCount] = info->mode[modeCount - 1];
            modeCount++;
         }
         newMode[modeCount] = '\0'; 
         changedMode = (atoi(newMode) == atoi(testMode)) ? 0 : 1;
         error = chmod(fileName, strtoul(newMode, NULL, 8));
      }
   }
   //printf("%s\n", info->mode);
   if (error != 0) {
      fprintf(stderr, "ERROR: %d\n", error);
      exit(error);
   }
   
   return changedMode;
}

void RecurseChange(char *dirName, int ops, Info *info) {
   int count = 0;
   DIR *dirPtr;
   //char buffer[1024];
   struct dirent *entPtr;
   struct stat fileStat;

   dirPtr = opendir(dirName);
   if (dirPtr != NULL) {
      while ((entPtr = readdir(dirPtr))) {
         stat(entPtr->d_name, &fileStat);
         if (S_ISDIR(fileStat.st_mode) && count > 1) {
            if (strcmp(entPtr->d_name, ".") != 0 || strcmp(entPtr->d_name, "..") != 0) {
               RecurseChange(entPtr->d_name, ops, info);
               ChangeMode(entPtr->d_name, ops, info);
               //printf("in\n");
            }
            //printf("%s\n", entPtr->d_name);
         }
         else {
            ChangeMode(entPtr->d_name, ops, info);
         }
         count++;
      }
   }
}

int main(int argc, char **argv) {
   int count = 0, changed, oldStderr;
   unsigned char ops = 0b010000;
   struct stat fileStat;
   Info *info;

   info = ProcessArgv(argc, argv, &ops);

   #ifdef DEBUG
   while (info->files[count] != NULL) {
      printf("%s\n", info->files[count++]);
   }
   printf("info->mode: %s\n", info->mode);
   count = 0;
   #endif

   if (ops & SILENT) {
      oldStderr = dup(2);
      freopen("/dev/null", "w", stderr);
      fclose(stderr);
   }

   if (ops & RECURSE) {
      if (ops & PRES && strcmp(info->files[0], "/") == 0) {
         fprintf(stderr, "Cannot recurse on root directory\n");
         exit(-1);
      }
      while (info->files[count] != NULL) {
         stat(info->files[count], &fileStat);
         if (S_ISDIR(fileStat.st_mode)) {
            RecurseChange(info->files[count], ops, info);
            ChangeMode(info->files[count], ops, info);
         }
         else {
            ChangeMode(info->files[count], ops, info);
         }
         count++;
      }
   }
   else {
      while (info->files[count] != NULL) {
         changed = ChangeMode(info->files[count], ops, info);
         if (ops & VERBOSE) {
            printf("mode of '%s' set to: %s\n", info->files[count], info->mode);
         }
         else if (ops & CHANGES) {
            if (changed) {
               printf("mode of '%s' set to: %s\n", info->files[count], info->mode);
            }
         }
         count++;
      }
   }

   

   return 0;
}
