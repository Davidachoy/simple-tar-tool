// Created by: David Achoy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>


typedef enum {
    ACTIVE,
    DELETED
} FileStatus;

typedef enum {
    VERBOSE_NONE,   // Sin reportes detallados
    VERBOSE_SIMPLE, // Reportes básicos (-v)
    VERBOSE_DETAILED// Reportes detallados (-vv)
} VerboseLevel;

VerboseLevel verbose_level = VERBOSE_NONE;

// Constants
typedef struct {
    int num_files;
    int total_size;
} ArchiveMetadata;

typedef struct {
    char filename[255];
    int file_size;
    int start_position;
    FileStatus status;   // Estado del archivo: activo o borrado.

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
void update (const char *archive_name, const char *file_name);
void append(char *archive_name, char *files[], int num_files);
void defragment(char *archive_name);
void verbose(char *archive_name);

bool find_file_info(FILE *archive, const char *file_name, FileInfo *file_info);
void mark_free_space(FILE *archive, int start_position, int size);

//auxiliary functions
bool find_file_info (
    FILE *archive, 
    const char *file_name, 
    FileInfo *file_info
) {
    // Posicionarse al inicio del archivo
    fseek(archive, 0, SEEK_SET);
    // Leer metadatos
    ArchiveMetadata metadata;
    if (fread(&metadata, sizeof(ArchiveMetadata), 1, archive) != 1) {
        printf("Error al leer metadatos.\n");
        return false;
    }
    // Buscar el archivo
    for (int i = 0; i < metadata.num_files; i++) {
        // Leer información de archivo
        if (fread(file_info, sizeof(FileInfo), 1, archive) != 1) {
            printf("Error al leer FileInfo en la iteración %d.\n", i);
            return false;
        }
        // Comparar nombre de archivo
        if (strcmp(file_info->filename, file_name) == 0) {
            return true;
        }
        // Saltar contenido de archivo
        if (fseek(archive, file_info->file_size, SEEK_CUR) != 0) {
            printf("Error al saltar contenido de archivo en la iteración %d.\n", i);
            return false;
        }
    }
    // No se encontró el archivo
    file_info->filename[0] = '\0';
    return false;
}

void mark_free_space(
    FILE *archive, 
    int start_position, 
    int size
) {
    fseek(archive, sizeof(ArchiveMetadata), SEEK_SET);
    FreeSpaceInfo space_info;
    int found = 0;
    while (fread(&space_info, sizeof(FreeSpaceInfo), 1, archive) == 1) {
        if (space_info.start_position + space_info.size == start_position) {
            space_info.size += size;
            fseek(archive, -sizeof(FreeSpaceInfo), SEEK_CUR);
            fwrite(&space_info, sizeof(FreeSpaceInfo), 1, archive);
            found = 1;
            break; 
        } else if (start_position + size == space_info.start_position){
            space_info.start_position = start_position;
            space_info.size += size;
            fseek(archive, -sizeof(FreeSpaceInfo), SEEK_CUR);
            fwrite(&space_info, sizeof(FreeSpaceInfo), 1, archive);
            found = 1;
            break;
        }
    }
    if (!found) {
        FreeSpaceInfo new_space;
        new_space.start_position = start_position;
        new_space.size = size;
        fwrite(&new_space, sizeof(FreeSpaceInfo), 1, archive);
    }   
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
        file_info.status = ACTIVE;
        file_info.start_position = ftell(archive) + sizeof(FileInfo);
        printf("file_info.start_position: %d\n", file_info.start_position);
        //print size of fileinfo
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
        if (file_info.status == ACTIVE) {
            printf("%s\n", file_info.filename);
        }
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

// delete function
void delete(
    char *archive_name, 
    const char *file_to_delete
) {
    // Abrir archivo
    FILE *archive = fopen(archive_name, "rb+");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    // Leer metadata
    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);
    // Buscar el archivo
    FileInfo file_info;
    if (!find_file_info(archive, file_to_delete, &file_info)) {
        printf("No se encontró el archivo %s\n", file_to_delete);
        fclose(archive);
        return;
    }

    // Cambiar el estado del archivo a DELETED
    file_info.status = DELETED;
    // Retroceder la posición actual del archivo para reescribir la información del archivo
    fseek(archive, -sizeof(FileInfo), SEEK_CUR);
    fwrite(&file_info, sizeof(FileInfo), 1, archive);
    //marcar el espacio como libre
    mark_free_space(archive, file_info.start_position, file_info.file_size);

    // Reposition cursor to the beginning of the file and write metadata
    fseek(archive, 0, SEEK_SET);
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archive);

    // Cerrar archivo
    fclose(archive);
}

void list_free_spaces(const char *archive_name) {
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    // Leer metadatos
    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);

    // Saltar información de archivos
    fseek(archive, metadata.num_files * sizeof(FileInfo), SEEK_CUR);

    // Mostrar espacios libres
    printf("Espacios libres:\n");
    FreeSpaceInfo space_info;
    while (fread(&space_info, sizeof(FreeSpaceInfo), 1, archive) == 1) {
        printf("Start position: %d, Size: %d\n", space_info.start_position, space_info.size);
    }

    fclose(archive);
}

void defragment(
    char *archive_name
) {

    FILE *archive = fopen(archive_name, "rb+");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }
    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);
    long read_index = sizeof(ArchiveMetadata);
    long write_index = sizeof(ArchiveMetadata);

    for (int i = 0; i < metadata.num_files; i++) {
        // Colocar el índice de lectura en la posición actual
        fseek(archive, read_index, SEEK_SET);
        FileInfo file_info;
        fread(&file_info, sizeof(FileInfo), 1, archive);
        if (file_info.status == ACTIVE) {

            // Cambiar la posición de inicio
            file_info.start_position = write_index + sizeof(FileInfo);
            
            // Colocar el índice de escritura en la posición correspondiente
            fseek(archive, write_index, SEEK_SET);
            fwrite(&file_info, sizeof(FileInfo), 1, archive);

            // Mover los datos del archivo
            char *buffer = malloc(file_info.file_size);
            fseek(archive, read_index + sizeof(FileInfo), SEEK_SET);
            fread(buffer, file_info.file_size, 1, archive);
            
            fseek(archive, write_index + sizeof(FileInfo), SEEK_SET);
            fwrite(buffer, file_info.file_size, 1, archive);
            free(buffer);

            // Ajustar el índice de escritura
            write_index += sizeof(FileInfo) + file_info.file_size;
        }
        read_index += sizeof(FileInfo) + file_info.file_size;
    }
    // Truncar el archivo
    ftruncate(fileno(archive), write_index);
    // Actualizar metadatos
    metadata.total_size = write_index;
    fseek(archive, 0, SEEK_SET);
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archive);
    fclose(archive);

}

void extractAll(
    const char *archive_name
) {
    // Abrir el archivo tar
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);

    // Recorrer y extraer todos los archivos
    for (int i = 0; i < metadata.num_files; i++) {
        FileInfo file_info;
        fread(&file_info, sizeof(FileInfo), 1, archive);

        // Verificar si el archivo está activo
        if (file_info.status == ACTIVE) {
            FILE *output = fopen(file_info.filename, "wb");
            if (!output) {
                printf("Error al abrir el archivo %s\n", file_info.filename);
            } else {
                // Leer y escribir el contenido del archivo
                fseek(archive, file_info.start_position, SEEK_SET);
                char buffer[1024];
                int bytes_left = file_info.file_size;

                while (bytes_left > 0) {
                    int bytes_to_read = bytes_left < sizeof(buffer) ? bytes_left : sizeof(buffer);
                    fread(buffer, bytes_to_read, 1, archive);
                    fwrite(buffer, bytes_to_read, 1, output);
                    bytes_left -= bytes_to_read;
                }

                fclose(output);
                printf("Archivo extraído: %s\n", file_info.filename);
            }
        }
    }

    // Cerrar el archivo tar
    fclose(archive);
}

void update(
    const char *archive_name, 
    const char *file_name
) {

    extractAll("archive.bin");

    //El Archivo no existe en el comprimido
    //if(){

    //}

    //Archivo existe en comprimido, no ha cambiado

    //Archivo existe en comprimido, ha cambiado

    //Archivo existe en comprimido, removerlo

}

int main() {
    char *files[] = {"text1.txt", "text2.txt"};

    //create("archive.tar", files, 2);
    //list("archive.tar");
    //extract("archive.tar", "text2.txt");
    //delete("archive.bin", "file1.txt");

    //update("file1.txt","archive.tar");
    extractAll("archive.tar");
    return 0;
/*
int main(int argc, char *argv[]) {
    int option;
    char *archive_name = NULL;
    bool verbose_flag = false;

    while ((option = getopt(argc, argv, "cx:t:du:r:pf:v")) != -1) {
        switch (option) {
            case 'c':
                archive_name = optarg;
                create(archive_name, &argv[optind], argc - optind);
                break;
            case 'x':
                archive_name = optarg;
                extract(archive_name, argv[optind]);
                break;
            case 't':
                archive_name = optarg;
                list(archive_name);
                break;
            case 'd':
                archive_name = optarg;
                delete(archive_name, argv[optind]);
                break;
            case 'u':
                archive_name = optarg;
                update(archive_name, &argv[optind], argc - optind);
                break;
            case 'r':
                archive_name = optarg;
                append(archive_name, &argv[optind], argc - optind);
                break;*/
 /*           case 'p':
                archive_name = optarg;
                defragment(archive_name);
                break;
            case 'v':
                if (verbose_flag) {
                    verbose_level = VERBOSE_DETAILED;
                } else {
                    verbose_level = VERBOSE_SIMPLE;
                    verbose_flag = true;
                }
                break;
            case 'f':
                archive_name = optarg;
                break;
            default:
                printf("Opción desconocida: %c\n", option);
                return 1;
        }
    }

    if (!archive_name) {
        printf("Error: No se especificó el archivo de salida.\n");
        return 1;
    }

    return 0; 
*/
}
