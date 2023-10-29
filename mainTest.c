// Created by: David Achoy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_FREE_SPACES 100  // Puede ser cualquier valor que consideres adecuado

// file status enum for file info
typedef enum {
    ACTIVE,
    DELETED
} FileStatus;

// verbose level enum for verbose
typedef enum {
    VERBOSE_NONE,   // Sin reportes detallados
    VERBOSE_SIMPLE, // Reportes básicos (-v)
    VERBOSE_DETAILED// Reportes detallados (-vv)
} VerboseLevel;

// Global variables for verbose
VerboseLevel verbose_level = VERBOSE_NONE;

// Structs for file info, archive metadata and free space info
typedef struct {
    int num_files;
    int total_size;
} ArchiveMetadata;

typedef struct {
    char filename[255];
    int file_size;
    int start_position;
    FileStatus status;

} FileInfo;

typedef struct {
    int start_position;
    int size;
} FreeSpaceInfo;



// Function Prototypes
void create(const char *archive_name, char *files[], int num_files); // create function            
void extract(const char *archive_name, const char *file_name); // extract function                     
void list(const char *archive_name); // list function
void delete(const char *archive_name, const char *file_to_delete); // delete function 
void update (const char *archive_name, char *files[] , int num_files);// update function
void append(char *archive_name, char *files[], int num_files); // append function
void defragment(char *archive_name); // defragment function
void verbose(char *archive_name); // verbose function

bool find_file_info(FILE *archive, const char *file_name, FileInfo *file_info); // auxiliary function
void mark_free_space(FILE *archive, int start_position, int size); // auxiliary function

void print_free_spaces(const char *archive_name) {
    // Abrir archivo
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    // Leer el número inicial de espacios libres
    int num_free_spaces;
    fread(&num_free_spaces, sizeof(int), 1, archive);
    printf("Número total de espacios libres: %d\n", num_free_spaces);

    // Leer y listar la información de los espacios libres
    for (int i = 0; i < num_free_spaces; i++) {
        FreeSpaceInfo free_space;
        if (fread(&free_space, sizeof(FreeSpaceInfo), 1, archive) != 1) {
            printf("Error al leer FreeSpaceInfo en la iteración %d.\n", i);
            break;
        }

        printf("Espacio libre %d:\n", i + 1);
        printf("\tPosición de inicio: %d\n", free_space.start_position);
        printf("\tTamaño: %d bytes\n", free_space.size);
    }

    fclose(archive);
}
void update_num_free_spaces(FILE *archive, int num_free_spaces) {
    fseek(archive, 0, SEEK_SET);  // Vuelve al inicio del archivo
    fwrite(&num_free_spaces, sizeof(int), 1, archive); // Escribe el valor actualizado
}


void load_free_spaces(FILE *archive, FreeSpaceInfo free_spaces[MAX_FREE_SPACES]) {
    // Posicionarse al inicio donde están los espacios libres.
    fseek(archive, sizeof(int), SEEK_SET);
    fread(free_spaces, sizeof(FreeSpaceInfo), MAX_FREE_SPACES, archive);
}
void save_free_spaces(FILE *archive, FreeSpaceInfo free_spaces[MAX_FREE_SPACES]) {
    // Posicionarse al inicio donde están los espacios libres.
    fseek(archive, sizeof(int), SEEK_SET);
    fwrite(free_spaces, sizeof(FreeSpaceInfo), MAX_FREE_SPACES, archive);
}
void insert_and_combine_free_space(FreeSpaceInfo free_spaces[MAX_FREE_SPACES], FreeSpaceInfo new_space) {
    // Insertar el nuevo espacio libre (esta parte puede mejorarse para mantener la lista ordenada si es necesario)
    for (int i = 0; i < MAX_FREE_SPACES; i++) {
        if (free_spaces[i].size == 0) { // Suponiendo que size == 0 indica un espacio no utilizado.
            free_spaces[i] = new_space;
            break;
        }
    }

    // Combinar espacios adyacentes
    for (int i = 0; i < MAX_FREE_SPACES - 1; i++) {
        for (int j = i + 1; j < MAX_FREE_SPACES; j++) {
            if (free_spaces[i].start_position + free_spaces[i].size == free_spaces[j].start_position) {
                // Espacios i y j son adyacentes
                free_spaces[i].size += free_spaces[j].size;
                free_spaces[j].size = 0; // Marcar como espacio no utilizado
            } else if (free_spaces[j].start_position + free_spaces[j].size == free_spaces[i].start_position) {
                // Espacios j e i son adyacentes
                free_spaces[j].size += free_spaces[i].size;
                free_spaces[i].size = 0; // Marcar como espacio no utilizado
            }
        }
    }
}

bool find_file_info (
    FILE *archive, 
    const char *file_name, 
    FileInfo *file_info
) {
    // Saltar el número inicial de espacios libres
    fseek(archive, sizeof(int), SEEK_CUR);

    // Saltar la lista de espacios libres
    fseek(archive, sizeof(FreeSpaceInfo) * MAX_FREE_SPACES, SEEK_CUR);

    // Leer metadatos del archivo
    ArchiveMetadata metadata;
    if (fread(&metadata, sizeof(ArchiveMetadata), 1, archive) != 1) {
        printf("Error al leer metadatos.\n");
        return false;
    }

    // Buscar el archivo en la lista de archivos
    for (int i = 0; i < metadata.num_files; i++) {
        // Leer información de archivo
        if (fread(file_info, sizeof(FileInfo), 1, archive) != 1) {
            printf("Error al leer FileInfo en la iteración %d.\n", i);
            return false;
        }

        // Mensaje de diagnóstico
        printf("Buscando: %s, Encontrado: %s\n", file_name, file_info->filename);

        // Comparar nombre de archivo
        if (strcmp(file_info->filename, file_name) == 0) {
            if (file_info->status == DELETED) {
                printf("El archivo %s fue encontrado pero está marcado como DELETED.\n", file_name);
                return false; // O true, dependiendo de tu caso de uso
            }
            return true;
        }

        // Saltar contenido de archivo para buscar el próximo FileInfo
        fseek(archive, file_info->file_size, SEEK_CUR);
    }

    // No se encontró el archivo
    file_info->filename[0] = '\0';
    return false;
}


void mark_free_space(
    FILE *archive, // Archivo tar
    int start_position, // Posición de inicio del espacio libre
    int size // Tamaño del espacio libre
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
    if (verbose_level >= VERBOSE_SIMPLE) {
        printf("\tArchivo %s abierto con éxito.\n", archive_name);
    }

    // Escribir el número inicial de espacios libres (0 al principio)
    int num_free_spaces = 0;
    fwrite(&num_free_spaces, sizeof(int), 1, archive);


    // Reservar espacio para la lista de espacios libres (por simplicidad, definamos un máximo)
    FreeSpaceInfo free_spaces[MAX_FREE_SPACES];
    memset(&free_spaces, 0, sizeof(FreeSpaceInfo) * MAX_FREE_SPACES); // Llenar con ceros
    fwrite(&free_spaces, sizeof(FreeSpaceInfo), MAX_FREE_SPACES, archive);

    // Escribir metadata
    ArchiveMetadata metadata = {num_files, 0};
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archive);
    if (verbose_level >= VERBOSE_DETAILED) {
        printf("\tMetadata escrito en el archivo.\n");
    }
    // Escribir información de archivos
    for (int i = 0; i < num_files; i++) {
        FILE *file = fopen(files[i], "rb");
        if (!file) {
            printf("Error al abrir el archivo %s\n", files[i]);
            return;
        }
        if (verbose_level >= VERBOSE_SIMPLE) {
            printf("\tArchivo %s abierto para lectura.\n", files[i]);
        }

        // Obtener tamaño del archivo
        fseek(file, 0, SEEK_END);
        int file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (verbose_level >= VERBOSE_DETAILED) {
            printf("\tTamaño del archivo %s: %d bytes.\n", files[i], file_size);
        }

        // Escribir información de archivo
        FileInfo file_info;
        strncpy(file_info.filename, files[i], 255);
        file_info.filename[255 - 1] = '\0';
        file_info.file_size = file_size;
        file_info.status = ACTIVE;
        file_info.start_position = ftell(archive) + sizeof(FileInfo);
        //print size of fileinfo
        fwrite(&file_info, sizeof(FileInfo), 1, archive);
        if (verbose_level >= VERBOSE_DETAILED) {
            printf("\tInformación del archivo %s escrita en el archivo de destino.\n", files[i]);
        }

        // Escribir contenido del archivo
        char *buffer = malloc(file_size);
        fread(buffer, file_size, 1, file);
        fwrite(buffer, file_size, 1, archive);
        if (verbose_level >= VERBOSE_SIMPLE) {
            printf("\tContenido del archivo %s escrito en el archivo de destino.\n", files[i]);
        }

        // Liberar memoria
        free(buffer);
        fclose(file);
    }
    if (verbose_level >= VERBOSE_SIMPLE) {
        printf("\tArchivo %s cerrado con éxito.\n", archive_name);
    }
    // Cerrar archivo
    fclose(archive);
}
void list(const char *archive_name) {
    // Abrir archivo
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    // Saltar el número inicial de espacios libres
    fseek(archive, sizeof(int), SEEK_CUR);

    // Saltar la lista de espacios libres
    fseek(archive, sizeof(FreeSpaceInfo) * MAX_FREE_SPACES, SEEK_CUR);

    // Leer metadata
    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);
    if (verbose_level >= VERBOSE_SIMPLE) {
        printf("\tLeyendo metadata del archivo...\n");
        printf("\tNúmero total de archivos en el archivo comprimido: %d\n", metadata.num_files);
    }

    int active_files_count = 0;

    // Listar archivos
    for (int i = 0; i < metadata.num_files; i++) {
        FileInfo file_info;
        size_t read = fread(&file_info, sizeof(FileInfo), 1, archive);

        // Si no se pudo leer más data, salir del loop
        if (read < 1) {
            break;
        }

        // Solo listar si el archivo está marcado como ACTIVE
        if (file_info.status == ACTIVE) {
            active_files_count++;
            printf("\tArchivo: %s\n", file_info.filename);
            if (verbose_level == VERBOSE_DETAILED) {
                printf("\tTamaño del archivo: %d bytes\n", file_info.file_size);
                printf("\tPosición de inicio en el archivo comprimido: %d\n", file_info.start_position);
            }
        }

        // Saltar contenido de archivo para llegar al próximo FileInfo
        fseek(archive, file_info.file_size, SEEK_CUR);
    }

    fclose(archive);

    if (verbose_level >= VERBOSE_SIMPLE) {
        printf("\tNúmero de archivos activos en el archivo comprimido: %d\n", active_files_count);
        printf("\tOperación de listar completada exitosamente.\n");
    }
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
void delete(const char *archive_name, const char *file_to_delete) {
    FILE *archive = fopen(archive_name, "rb+");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    // Buscar el archivo
    FileInfo file_info;
    if(!find_file_info(archive, file_to_delete, &file_info)) {
        printf("El archivo %s no fue encontrado en el archivo.\n", file_to_delete);
        fclose(archive);
        return;
    }

    if(file_info.status == DELETED) {
        printf("El archivo %s ya estaba marcado como borrado.\n", file_to_delete);
        fclose(archive);
        return;
    }

    // Cambiar el estado a DELETED
    file_info.status = DELETED;
    fseek(archive, -sizeof(FileInfo), SEEK_CUR); // Regresar para reescribir el FileInfo
    fwrite(&file_info, sizeof(FileInfo), 1, archive);

    // Marcar el espacio ocupado por el archivo como espacio libre
    FreeSpaceInfo new_free_space;
    new_free_space.start_position = file_info.start_position;
    new_free_space.size = file_info.file_size;

    FreeSpaceInfo free_spaces[MAX_FREE_SPACES];
    // Suponiendo que tienes una función para cargar todos los espacios libres existentes:
    load_free_spaces(archive, free_spaces);

    // Insertar el nuevo espacio libre, y si es posible, combinar con espacios libres adyacentes
    insert_and_combine_free_space(free_spaces, new_free_space);

    // Suponiendo que tienes una función para guardar todos los espacios libres después de modificarlos:
    save_free_spaces(archive, free_spaces);

    fclose(archive);
    printf("El archivo %s ha sido marcado como eliminado.\n", file_to_delete);
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
    const char *archive_name // Nombre del archivo tar
) {
    // Abrir el archivo tar
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }
    if (verbose_level >= VERBOSE_SIMPLE) {
        printf("\tArchivo %s abierto con éxito.\n", archive_name);
    }

    // Saltar el número inicial de espacios libres
    fseek(archive, sizeof(int), SEEK_CUR);

    // Saltar la lista de espacios libres
    fseek(archive, sizeof(FreeSpaceInfo) * MAX_FREE_SPACES, SEEK_CUR);

    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);
    if (verbose_level >= VERBOSE_DETAILED) {
        printf("\tMetadata leída del archivo.\n");
    }

    // Recorrer y extraer todos los archivos
    for (int i = 0; i < metadata.num_files; i++) {
        FileInfo file_info;
        fread(&file_info, sizeof(FileInfo), 1, archive);

        if (verbose_level >= VERBOSE_DETAILED) {
            printf("\tLeyendo información del archivo %d de %d.\n", i+1, metadata.num_files);
        }

        // Verificar si el archivo está activo
        if (file_info.status == ACTIVE) {
            FILE *output = fopen(file_info.filename, "wb");
            if (!output) {
                printf("Error al abrir el archivo %s\n", file_info.filename);
            } else {
                if (verbose_level >= VERBOSE_SIMPLE) {
                    printf("\tArchivo %s abierto para escritura.\n", file_info.filename);
                }

                // Leer y escribir el contenido del archivo
                char buffer[1024];
                int bytes_left = file_info.file_size;

                while (bytes_left > 0) {
                    int bytes_to_read = bytes_left < sizeof(buffer) ? bytes_left : sizeof(buffer);
                    fread(buffer, bytes_to_read, 1, archive);
                    fwrite(buffer, bytes_to_read, 1, output);
                    bytes_left -= bytes_to_read;
                }

                fclose(output);
                if (verbose_level >= VERBOSE_SIMPLE) {
                    printf("\tArchivo extraído: %s\n", file_info.filename);
                }
            }
        } else {
            // Si el archivo no está activo, aún debes saltar su contenido para procesar el siguiente archivo
            fseek(archive, file_info.file_size, SEEK_CUR);
        }
    }
    if (verbose_level >= VERBOSE_SIMPLE) {
        printf("\tArchivo %s cerrado con éxito.\n", archive_name);
    }

    // Cerrar el archivo tar
    fclose(archive);
}


void extractAllToFolder(const char *archive_name, const char *destination_folder) {
    // Abrir el archivo tar
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archive);

    // Crear la ruta completa de destino
    char full_destination_path[256];
    snprintf(full_destination_path, sizeof(full_destination_path), "%s/", destination_folder);

    // Recorrer y extraer todos los archivos
    for (int i = 0; i < metadata.num_files; i++) {
        FileInfo file_info;
        fread(&file_info, sizeof(FileInfo), 1, archive);

        // Verificar si el archivo está activo
        if (file_info.status == ACTIVE) {
            char full_file_path[512];
            snprintf(full_file_path, sizeof(full_file_path), "%s%s", full_destination_path, file_info.filename);
            FILE *output = fopen(full_file_path, "wb");
            if (!output) {
                printf("Error al abrir el archivo %s\n", full_file_path);
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
                printf("Archivo extraído: %s\n", full_file_path);
            }
        }
    }

    // Cerrar el archivo tar
    fclose(archive);
}

int archivoExisteEnCarpeta(const char *archive, const char *file) {
    char ruta_completa[256]; // Ajusta el tamaño según tus necesidades
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", archive, file);

    if (access(ruta_completa, F_OK) != -1) {
        return 1; // El archivo existe en la carpeta
    } else {
        return 0; // El archivo no existe en la carpeta
    }
}

void createFromFolderAndFile(const char *archive_name, const char *folder_name, const char *additional_file) {
    // Abrir el archivo .tar
    FILE *archive = fopen(archive_name, "wb");
    if (!archive) {
        printf("Error al abrir el archivo %s\n", archive_name);
        return;
    }

    // Escribir metadata (inicialmente con 0 archivos)
    ArchiveMetadata metadata = {0, 0};
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archive);

    // Abrir la carpeta
    DIR *folder = opendir(folder_name);
    if (!folder) {
        printf("Error al abrir la carpeta %s\n", folder_name);
        fclose(archive);
        return;
    }

    // Recorrer los archivos en la carpeta
    struct dirent *entry;
    while ((entry = readdir(folder)) != NULL) {
        if (entry->d_type == DT_REG) { // Solo archivos regulares
            char full_path[512]; // Ajusta el tamaño según tus necesidades
            snprintf(full_path, sizeof(full_path), "%s/%s", folder_name, entry->d_name);

            FILE *file = fopen(full_path, "rb");
            if (!file) {
                printf("Error al abrir el archivo %s\n", full_path);
                continue;
            }

            // Obtener tamaño del archivo
            fseek(file, 0, SEEK_END);
            int file_size = ftell(file);
            fseek(file, 0, SEEK_SET);

            // Escribir información del archivo en el archivo .tar
            FileInfo file_info;
            strncpy(file_info.filename, entry->d_name, 255);
            file_info.filename[255 - 1] = '\0';
            file_info.file_size = file_size;
            file_info.status = ACTIVE;
            file_info.start_position = ftell(archive) + sizeof(FileInfo);
            fwrite(&file_info, sizeof(FileInfo), 1, archive);

            // Escribir contenido del archivo
            char *buffer = malloc(file_size);
            fread(buffer, file_size, 1, file);
            fwrite(buffer, file_size, 1, archive);

            // Liberar memoria y cerrar el archivo
            free(buffer);
            fclose(file);

            // Actualizar el número de archivos en la metadata
            metadata.num_files++;
        }
    }

    // Agregar el archivo adicional al archivo .tar
    FILE *extra_file = fopen(additional_file, "rb");
    if (extra_file) {
        fseek(extra_file, 0, SEEK_END);
        int extra_file_size = ftell(extra_file);
        fseek(extra_file, 0, SEEK_SET);

        FileInfo extra_file_info;
        strncpy(extra_file_info.filename, additional_file, 255);
        extra_file_info.filename[255 - 1] = '\0';
        extra_file_info.file_size = extra_file_size;
        extra_file_info.status = ACTIVE;
        extra_file_info.start_position = ftell(archive) + sizeof(FileInfo);
        fwrite(&extra_file_info, sizeof(FileInfo), 1, archive);

        char *extra_buffer = malloc(extra_file_size);
        fread(extra_buffer, extra_file_size, 1, extra_file);
        fwrite(extra_buffer, extra_file_size, 1, archive);

        free(extra_buffer);
        fclose(extra_file);

        metadata.num_files++;
    } else {
        printf("No se pudo abrir el archivo adicional %s\n", additional_file);
    }

    // Regresar al principio del archivo para actualizar la metadata
    fseek(archive, 0, SEEK_SET);
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archive);

    // Cerrar la carpeta y el archivo .tar
    closedir(folder);
    fclose(archive);
}

int eliminarCarpetaRecursiva(const char *carpeta) {
    DIR *d = opendir(carpeta);
    size_t carpeta_len = strlen(carpeta);
    int resultado = 1;

    if (!d) {
        perror("Error al abrir la carpeta");
        return 0;
    }

    struct dirent *p;
    while ((p = readdir(d)) != NULL) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
            continue;
        }

        size_t len = carpeta_len + strlen(p->d_name) + 2;
        char *buf = (char *)malloc(len);

        if (buf) {
            snprintf(buf, len, "%s/%s", carpeta, p->d_name);

            struct stat statbuf;
            if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode)) {
                    resultado = eliminarCarpetaRecursiva(buf);
                } else {
                    resultado = remove(buf);
                }
            }

            free(buf);
        }
    }

    closedir(d);

    if (resultado) {
        resultado = rmdir(carpeta);
    }

    return resultado;
}

void update(
    const char *archive_name, 
    char *files[],  
    int num_files
) {
    //crea un folder con los archivos
    mkdir("temporal", 0777);
    extractAllToFolder(archive_name, "temporal");
    //remove(archive_name);

     for (int i = 0; i < num_files; i++) {
        int existe = archivoExisteEnCarpeta("temporal", files[i]);
        createFromFolderAndFile(archive_name, "temporal", files[i]);
        printf("Agregado: %s\n", files[i]);
        eliminarCarpetaRecursiva("./temporal");
        remove("./temporal");
    }
}
// auxiliar functions for main

//show valid optiones function
void showValidOptions() {
    printf("Uso: ./star <opciones> <archivoSalida> <archivo1> <archivo2> ... <archivoN>\n\n");
    printf("Descripción: Esta herramienta permite realizar diferentes operaciones sobre archivos, tales como crear, extraer, listar y actualizar. A continuación, se presentan las opciones disponibles:\n\n");

    printf("Opciones principales:\n");
    printf("\t-c, --create : Crea un nuevo archivo comprimido con los archivos especificados.\n");
    printf("\t-x, --extract : Extrae los contenidos de un archivo comprimido a la ubicación actual.\n");
    printf("\t-t, --list : Lista los contenidos de un archivo comprimido, mostrando detalles de cada archivo contenido.\n");
    printf("\t--delete : Borra un archivo o archivos específicos dentro de un archivo comprimido.\n");
    printf("\t-u, --update : Actualiza el contenido del archivo comprimido con nuevos archivos o versiones de archivos existentes.\n");
    printf("\t-v, --verbose : Proporciona un reporte detallado de las acciones que se están realizando. Use -v para un reporte básico y -vv para un reporte detallado.\n");
    printf("\t-r, --append : Agrega contenido a un archivo comprimido sin eliminar o modificar el contenido existente.\n");
    printf("\t-p, --pack : Desfragmenta el contenido del archivo comprimido, eliminando espacios vacíos y optimizando el almacenamiento.\n\n");

    printf("Ejemplos de uso:\n");
    printf("\t./star -c archivoSalida.tar archivo1.txt archivo2.txt\n");
    printf("\t./star --list archivoSalida.tar\n");
    printf("\t./star -v --delete archivoSalida.tar archivo1.txt\n");

}


int main(int argc, char *argv[]) {
    int options_count = 0; // Contador de opciones
    int optionsLenght; // Largo de las opciones
    char *archive_name; // Nombre del archivo
    char **files_name; // Nombre de los archivos
    int num_files; // Número de archivos

    // Verificar la cantidad adecuada de parámetros
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        showValidOptions();
        return 0;
    }
    // Verificar la cantidad adecuada de parámetros
    if (argc < 3) {
        printf("Uso: ./star <opciones> <archivoSalida> <archivo1> <archivo2> ... <archivoN>\n");
        return 1;
    }
    // Verificar opciones
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') break;

        if (argv[i][1] != '-') {
            optionsLenght = strlen(argv[i]);

            for (int j = 1; j < optionsLenght; j++) {
                 if (!(strchr("cvxturp", argv[i][j]))) {
                    printf("Opción no válida: %c\n", argv[i][j]);
                    printf("Para ver una lista de comandos disponibles, ingrese --help.\n");
                    return 1;
                }
                if (argv[i][j] == 'v') {
                    if (verbose_level == VERBOSE_NONE) {
                        verbose_level = VERBOSE_SIMPLE;
                    } else if (verbose_level == VERBOSE_SIMPLE) {
                        verbose_level = VERBOSE_DETAILED;
                    }
                }
            }
        }
        if (strcmp(argv[i], "--verbose") == 0) {
            if (verbose_level == VERBOSE_NONE) {
                verbose_level = VERBOSE_SIMPLE;
            } else if (verbose_level == VERBOSE_SIMPLE) {
                verbose_level = VERBOSE_DETAILED;
            }
        }
        options_count++;
    }
    // Verificar la cantidad adecuada de parámetros
    if(argc < options_count + 2){
        printf("Uso: ./star <opciones> <archivoSalida> <archivo1> <archivo2> ... <archivoN>\n");
        return 1;
    }
    //get file name
    archive_name = argv[options_count + 1];
    //get files name
    files_name = &argv[options_count + 2];
    //get number of files
    num_files = argc - options_count - 2;
    // Verificar opciones
    for(int i = 0; i < options_count; i++){
       if (argv[i+1][1] == '-'){
            if (strcmp(argv[i+1], "--create") == 0) {
                printf("create\n");
                create(archive_name, files_name, num_files);
            } else if (strcmp(argv[i+1], "--extract") == 0){
                printf("extract\n");
                extractAll(archive_name);
            } else if (strcmp(argv[i+1], "--list") == 0){
                printf("list\n");
                list(archive_name);
            } else if (strcmp(argv[i+1], "--delete") == 0){
                printf("delete\n");
                delete(archive_name, files_name[0]);
            } else if (strcmp(argv[i+1], "--update") == 0){
                printf("update\n");
                update(archive_name, files_name, num_files);
            } else if (strcmp(argv[i+1], "--append") == 0){
                printf("append\n");
            } else if (strcmp(argv[i+1], "--pack") == 0){
                printf("pack\n");
            }else if(strcmp(argv[i+1], "--help")==0){
                showValidOptions();
            } else {
                printf("Opción no válida: %s\n", argv[i+1]);
                printf("Para ver una lista de comandos disponibles, ingrese --help.\n");
            }
        } else {
            for (int j = 1; j < optionsLenght; j++) {
                switch (argv[i+1][j]){
                    case 'c':
                        printf("create\n");
                        create(archive_name, files_name, num_files);
                        break;
                    case 'x':
                        printf("extract\n");
                        extractAll(archive_name);
                        break;
                    case 't':
                        printf("list\n");
                        list(archive_name);
                        break;
                    case 'u':
                        printf("update\n");
                        update(archive_name, files_name, num_files);
                        printf("updated.\n");
                        break;
                    case 'r':
                        print_free_spaces(archive_name);
                        printf("pack\n");
                        break;
                    case 'v':
                        break;
                    case 'p':
                        printf("append\n");
                        break;
                    default:
                        printf("Opción no válida: %c\n", argv[i+1][j]);
                        printf("Para ver una lista de comandos disponibles, ingrese --help.\n");
                        break;
                }
            }
        }
    }
    //print verbose level
    switch (verbose_level) {
        case VERBOSE_NONE:
            printf("Nivel de detalle: Ninguno\n");
            break;
        case VERBOSE_SIMPLE:
            printf("Nivel de detalle: Simple\n");
            break;
        case VERBOSE_DETAILED:
            printf("Nivel de detalle: Detallado\n");
            break;
    }
    return 0;
}


/*
    Pendientes
    #1. Implementar append
    #2. Implementar pack
    #3. Implementar delete
    #4. Ver cuales tienen Verbose
    #5. Pruebas
*/