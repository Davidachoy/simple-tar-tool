// Created by: David Achoy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Constants
typedef struct {
    int num_files;
    int total_size;
} ArchiveMetadata;

typedef struct {
    char filename[255];
    int file_size;
    int start_position;
} FileInfo;

typedef struct {
    int start_position;
    int size;
} FreeSpaceInfo;

// Function Prototypes
void create(const char *archive_name, char *files[], int num_files);
void extract(char *archive_name);
void list(char *archive_name);
void delete(char *archive_name, const char *file_to_delete);
void update (char *archive_name, char *files[], int num_files);
void append(char *archive_name, char *files[], int num_files);
void defragment(char *archive_name);
void verbose(char *archive_name);

// create function

void create(
    const char *archive_name,  // Nombre del archivo de destino
    char *files[],             // Arreglo de nombres de archivos para incluir en el archivo
    int num_files              // Número de archivos en el arreglo
) {
    // Abrir archivo
    FILE *archive = fopen(archive_name, "wb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }
    // Escribir metadata
    ArchiveMetadata metadata = {num_files, 0};
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archive);

    // Escribir información de archivos
    for (int i = 0; i < num_files; i++) {
        FILE *file = fopen(files[i], "rb");
        if (!file) {
            printf("Error al abrir el archivo %s\n", files[i]);
            continue;;
        }
        // Obtener tamaño del archivo
        fseek(file, 0, SEEK_END);
        int file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        // Escribir información de archivo
        FileInfo file_info = {files[i], file_size, ftell(archive)};
        fwrite(&file_info, sizeof(FileInfo), 1, archive);
        // Escribir contenido del archivo
        char *buffer = malloc(file_size);
        fread(buffer, file_size, 1, file);
        fwrite(buffer, file_size, 1, archive);
        // Liberar memoria
        free(buffer);
        fclose(file);
    }
    // Cerrar archivo
    fclose(archive);
}
int main () {
    return 0;
}