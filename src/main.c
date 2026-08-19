#include <stdio.h>
#include <string.h>
#include "../include/xmlbuilder.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.xml> [database.db] [output.sql]\n", argv[0]);
        return 1;
    }

    const char *xml_file = argv[1];
    const char *db_name = argc > 2 ? argv[2] : "my_database.db";
    const char *sql_file = argc > 3 ? argv[3] : "output.sql";

    if (strstr(xml_file, ".xml") == NULL) {
        fprintf(stderr, "Error: the file must have the .xml extension\n");
        return 1;
    }

    parseXml(xml_file, sql_file);

    return executeSqlFile(db_name, sql_file);
}
