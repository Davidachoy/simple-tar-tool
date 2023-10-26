# simple-tar-tool
Una implementación en C del comando tar para ambientes UNIX. Este proyecto proporciona funcionalidades básicas para empacar, desempacar, listar y administrar archivos en un formato personalizado.


## revisar 

linea 126 void create

Para cuando el usuario ingresa -v 


    if (verbose_level >= VERBOSE_SIMPLE) {
        printf("\tArchivo %s abierto con éxito.\n", archive_name);
    }

Para cuando el usuario ingresa -vv

    if (verbose_level >= VERBOSE_DETAILED) {
        printf("\tMetadata escrito en el archivo.\n");
    }