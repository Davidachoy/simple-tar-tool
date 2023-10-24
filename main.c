// Created by: David Achoy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
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
void extract(const char *archive_name, const char *file_name);
void list(const char *archive_name);
void delete(char *archive_name, const char *file_to_delete);
void update (char *archive_name, char *files[], int num_files);
void append(char *archive_name, char *files[], int num_files);
void defragment(char *archive_name);
void verbose(char *archive_name);
bool find_file_info(FILE *archive, const char *file_name, FileInfo *file_info);
//auxiliary functions
bool find_file_info (
    FILE *archive, 
    const char *file_name, 
    FileInfo *file_info
) {
    // leer metadata
    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);
    // buscar el archivo
    for (int i = 0; i < metadata.num_files; i++) {
        // leer información de archivo
        fread(file_info, sizeof(FileInfo), 1, archive);
        // comparar nombre de archivo
        if (strcmp(file_info->filename, file_name) == 0) {
            return true;
        }
        // saltar contenido de archivo
        fseek(archive, file_info->file_size, SEEK_CUR);
    }
    // no se encontró el archivo
    file_info->filename[0] = '\0';
    return false;
}
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
        FileInfo file_info;
        strncpy(file_info.filename, files[i], 255);
        file_info.filename[255 - 1] = '\0';
        file_info.file_size = file_size;
        file_info.start_position = ftell(archive);
        printf("file_info.start_position: %d\n", file_info.start_position);
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

// list function
void list(
    const char *archive_name
) {
    // Abrir archivo
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }
    // Leer metadata
    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);

    //listar archivos
    for (int i = 0; i < metadata.num_files; i++) {
        // Leer información de archivo
        FileInfo file_info;
        fread(&file_info, sizeof(FileInfo), 1, archive);
        // Imprimir nombre de archivo
        printf("%s\n", file_info.filename);
        // Saltar contenido de archivo
        fseek(archive, file_info.file_size, SEEK_CUR);
    }
    fclose(archive);
}

// extract function
void extract(
    const char *archive_name, 
    const char *file_name
) {
    // Abrir archivo
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    // buscar el archivo
    FileInfo file_info;
    if(!find_file_info(archive, file_name, &file_info)) {
        printf("No se encontró el archivo %s\n", file_name);
        fclose(archive);
        return;
    }
    FILE *output = fopen(file_name, "wb");
    if (!output) {
        printf("Error al abrir el archivo %s\n", file_name);
        fclose(archive);
        return;
    }

    // Leer contenido de archivo
    fseek(archive, file_info.start_position, SEEK_SET);
    char buffer[1024];
    int bytes_left = file_info.file_size;

    // Escribir contenido de archivo
    while (bytes_left > 0)
    {
        int bytes_to_read = bytes_left < sizeof(buffer) ? bytes_left : sizeof(buffer);
        fread(buffer, bytes_to_read, 1, archive);
        fwrite(buffer, bytes_to_read, 1, output);
        bytes_left -= bytes_to_read;
    }
    // Cerrar archivos
    fclose(archive);
    fclose(output);
}

int main() {
    char *files[] = {"file1.txt", "file2.txt"};
    create("archive.bin", files, 2);
    return 0;
}
