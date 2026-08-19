#include "../include/xmlbuilder.h"

RelationAttributes relation_attributes[MAX_RELATIONS];
TableID id_counters[MAX_TABLES];
int relation_count = 0;
int id_counter_count = 0;

int isWhitespaceOnly(const char *str) {
    while (*str) {
        if (*str != ' ' && *str != '\n' && *str != '\t' && *str != '\r') return 0;
        str++;
    }
    return 1;
}

void escapeSqlString(const char *input, char *output, size_t max_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < max_size - 1; i++) {
        if (input[i] == '\'') {
            if (j < max_size - 2) {
                output[j++] = '\'';
                output[j++] = '\'';
            }
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

int getNewId(const char *table_name) {
    for (int i = 0; i < id_counter_count; i++) {
        if (strcmp(id_counters[i].table_name, table_name) == 0) {
            return ++id_counters[i].current_id;
        }
    }

    strncpy(id_counters[id_counter_count].table_name, table_name, 256);
    id_counters[id_counter_count].current_id = 1;
    return id_counters[id_counter_count++].current_id;
}

int tableExists(const char *table_name, char tables[MAX_TABLES][MAX_TABLE_NAME], int table_count) {
    for (int i = 0; i < table_count; i++) {
        if (strcmp(tables[i], table_name) == 0) return 1;
    }
    return 0;
}

int isForeignKey(xmlNodePtr node) {
    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) return 1;
    }
    return 0;
}

int hasChildElements(xmlNodePtr node) {
    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) return 1;
    }
    return 0;
}

int countSameName(xmlNodePtr node, const char *name) {
    int count = 0;
    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && strcmp((char *)child->name, name) == 0) {
            count++;
        }
    }
    return count;
}

int executeSqlFile(const char *db_name, const char *sql_file) {
    sqlite3 *db;
    char *err_msg = 0;
    FILE *file;
    char *sql;
    long length;

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Unable to open the database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    file = fopen(sql_file, "r");
    if (!file) {
        fprintf(stderr, "Unable to open the SQL file: %s\n", sql_file);
        sqlite3_close(db);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    sql = malloc(length + 1);
    if (sql == NULL) {
        fprintf(stderr, "Error: memory allocation failed.\n");
        fclose(file);
        sqlite3_close(db);
        return 1;
    }

    fread(sql, 1, length, file);
    sql[length] = '\0';
    fclose(file);

    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL execution failed: %s\n", err_msg);
        sqlite3_free(err_msg);
        free(sql);
        sqlite3_close(db);
        return 1;
    }

    free(sql);
    sqlite3_close(db);

    printf("Successfully executed the SQL file in %s\n", db_name);
    return 0;
}

void addRelation(const char *table1, const char *table2, xmlNodePtr relation_node) {
    for (int i = 0; i < relation_count; i++) {
        if ((strcmp(relation_attributes[i].table_1, table1) == 0 &&
             strcmp(relation_attributes[i].table_2, table2) == 0) ||
            (strcmp(relation_attributes[i].table_1, table2) == 0 &&
             strcmp(relation_attributes[i].table_2, table1) == 0)) {
            return;
        }
    }

    strncpy(relation_attributes[relation_count].table_1, table1, 256);
    strncpy(relation_attributes[relation_count].table_2, table2, 256);
    relation_attributes[relation_count].attr_count = 0;

    for (xmlNodePtr attr_node = relation_node->children; attr_node != NULL; attr_node = attr_node->next) {
        if (attr_node->type == XML_ELEMENT_NODE) {
            int exists = 0;
            for (int j = 0; j < relation_attributes[relation_count].attr_count; j++) {
                if (strcmp(relation_attributes[relation_count].attributes[j], (char *)attr_node->name) == 0) {
                    exists = 1;
                    break;
                }
            }
            if (!exists && relation_attributes[relation_count].attr_count < 10) {
                strncpy(relation_attributes[relation_count].attributes[relation_attributes[relation_count].attr_count], (char *)attr_node->name, 256);
                relation_attributes[relation_count].attr_count++;
            }
        }
    }
    relation_count++;
}

void createTable(FILE *output_file, xmlNodePtr node, char tables[MAX_TABLES][MAX_TABLE_NAME], int *table_count) {
    if (!hasChildElements(node)) return;

    if (tableExists((char *)node->name, tables, *table_count)) return;

    strncpy(tables[*table_count], (char *)node->name, MAX_TABLE_NAME);
    (*table_count)++;

    fprintf(output_file, "CREATE TABLE IF NOT EXISTS %s (\n", node->name);
    fprintf(output_file, "    id_%s INTEGER PRIMARY KEY", node->name);

    for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
        if (attr->name != NULL) {
            fprintf(output_file, ",\n    %s varchar(42)", attr->name);
        }
    }

    int fk_count = 0;

    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            int repeated = countSameName(node, (char *)child->name) > 1;
            if (isForeignKey(child) || repeated) {
                addRelation((char *)node->name, (char *)child->name, child);
                fk_count++;
            } else if (!hasChildElements(child) && child->properties != NULL) {
                for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next) {
                    if (attr->name != NULL) {
                        fprintf(output_file, ",\n    %s_%s varchar(42)", child->name, attr->name);
                    }
                }
            } else {
                fprintf(output_file, ",\n    %s varchar(42)", child->name);
            }
        }
    }
    (void)fk_count;

    fprintf(output_file, "\n);\n\n");

    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            if (hasChildElements(child)) {
                createTable(output_file, child, tables, table_count);
            } else if (countSameName(node, (char *)child->name) > 1) {
                createLeafTable(output_file, child, tables, table_count);
            }
        }
    }
}

void createLeafTable(FILE *output_file, xmlNodePtr node, char tables[MAX_TABLES][MAX_TABLE_NAME], int *table_count) {
    if (tableExists((char *)node->name, tables, *table_count)) return;

    strncpy(tables[*table_count], (char *)node->name, MAX_TABLE_NAME);
    (*table_count)++;

    fprintf(output_file, "CREATE TABLE IF NOT EXISTS %s (\n", node->name);
    fprintf(output_file, "    id_%s INTEGER PRIMARY KEY", node->name);

    if (node->properties != NULL) {
        for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
            if (attr->name != NULL) {
                fprintf(output_file, ",\n    %s varchar(42)", attr->name);
            }
        }
    } else {
        fprintf(output_file, ",\n    value varchar(42)");
    }

    fprintf(output_file, "\n);\n\n");
}

void insertData(FILE *output_file, xmlNodePtr node, int parent_id, const char *parent_table) {
    if (node->type != XML_ELEMENT_NODE) return;

    int new_id = getNewId((char *)node->name);
    fprintf(output_file, "INSERT INTO %s (id_%s", node->name, node->name);

    for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
        if (attr->name != NULL) {
            fprintf(output_file, ", %s", attr->name);
        }
    }

    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && !isForeignKey(child) &&
            countSameName(node, (char *)child->name) == 1) {
            if (!hasChildElements(child) && child->properties != NULL) {
                for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next) {
                    if (attr->name != NULL) {
                        fprintf(output_file, ", %s_%s", child->name, attr->name);
                    }
                }
            } else {
                fprintf(output_file, ", %s", child->name);
            }
        }
    }

    fprintf(output_file, ") VALUES (%d", new_id);

    for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
        if (attr->name != NULL) {
            xmlChar *attr_value = xmlGetProp(node, attr->name);

            if (attr_value != NULL) {
                char escaped_value[512];
                escapeSqlString((char *)attr_value, escaped_value, sizeof(escaped_value));
                fprintf(output_file, ", '%s'", escaped_value);
                xmlFree(attr_value);
            } else {
                fprintf(output_file, ", NULL");
            }
        }
    }

    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && !isForeignKey(child) &&
            countSameName(node, (char *)child->name) == 1) {
            if (!hasChildElements(child) && child->properties != NULL) {
                for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next) {
                    xmlChar *attr_value = xmlGetProp(child, attr->name);
                    if (attr_value != NULL) {
                        char escaped_value[512];
                        escapeSqlString((char *)attr_value, escaped_value, sizeof(escaped_value));
                        fprintf(output_file, ", '%s'", escaped_value);
                        xmlFree(attr_value);
                    } else {
                        fprintf(output_file, ", NULL");
                    }
                }
            } else {
                xmlChar *content = xmlNodeGetContent(child);
                if (content != NULL) {
                    char escaped_content[512];
                    escapeSqlString((char *)content, escaped_content, sizeof(escaped_content));
                    fprintf(output_file, ", '%s'", escaped_content);
                    xmlFree(content);
                } else {
                    fprintf(output_file, ", NULL");
                }
            }
        }
    }

    fprintf(output_file, ");\n");

    if (parent_id > 0 && parent_table != NULL) {
        fprintf(output_file,
                "INSERT INTO %s_%s (id_%s, id_%s) VALUES (%d, %d);\n",
                parent_table, node->name, parent_table, node->name, parent_id, new_id);
    }

    for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            int repeated = countSameName(node, (char *)child->name) > 1;
            if (isForeignKey(child)) {
                insertData(output_file, child, new_id, node->name);
            } else if (repeated) {
                insertLeafData(output_file, child, new_id, node->name);
            }
        }
    }
}

void insertLeafData(FILE *output_file, xmlNodePtr node, int parent_id, const char *parent_table) {
    int new_id = getNewId((char *)node->name);
    fprintf(output_file, "INSERT INTO %s (id_%s", node->name, node->name);

    if (node->properties != NULL) {
        for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
            if (attr->name != NULL) {
                fprintf(output_file, ", %s", attr->name);
            }
        }
    } else {
        fprintf(output_file, ", value");
    }

    fprintf(output_file, ") VALUES (%d", new_id);

    if (node->properties != NULL) {
        for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
            if (attr->name != NULL) {
                xmlChar *attr_value = xmlGetProp(node, attr->name);
                if (attr_value != NULL) {
                    char escaped_value[512];
                    escapeSqlString((char *)attr_value, escaped_value, sizeof(escaped_value));
                    fprintf(output_file, ", '%s'", escaped_value);
                    xmlFree(attr_value);
                } else {
                    fprintf(output_file, ", NULL");
                }
            }
        }
    } else {
        xmlChar *content = xmlNodeGetContent(node);
        if (content != NULL) {
            char escaped_content[512];
            escapeSqlString((char *)content, escaped_content, sizeof(escaped_content));
            fprintf(output_file, ", '%s'", escaped_content);
            xmlFree(content);
        } else {
            fprintf(output_file, ", NULL");
        }
    }

    fprintf(output_file, ");\n");

    fprintf(output_file,
            "INSERT INTO %s_%s (id_%s, id_%s) VALUES (%d, %d);\n",
            parent_table, node->name, parent_table, node->name, parent_id, new_id);
}

void generateSql(xmlNodePtr root, const char *output_path) {
    if (root == NULL) return;

    FILE *output_file = fopen(output_path, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error: unable to open %s for writing.\n", output_path);
        return;
    }

    char tables[MAX_TABLES][MAX_TABLE_NAME];
    int table_count = 0;

    for (xmlNodePtr node = root->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            createTable(output_file, node, tables, &table_count);

            for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
                if (child->type == XML_ELEMENT_NODE && isForeignKey(child)) {
                    addRelation((char *)node->name, (char *)child->name, child);
                }
            }
        }
    }

    for (int i = 0; i < relation_count; i++) {
        fprintf(output_file, "CREATE TABLE IF NOT EXISTS %s_%s (\n", relation_attributes[i].table_1, relation_attributes[i].table_2);
        fprintf(output_file, "    id_%s_%s INTEGER PRIMARY KEY,\n", relation_attributes[i].table_1, relation_attributes[i].table_2);
        fprintf(output_file, "    id_%s INTEGER,\n", relation_attributes[i].table_1);
        fprintf(output_file, "    id_%s INTEGER", relation_attributes[i].table_2);
        fprintf(output_file, ",\n    FOREIGN KEY (id_%s) REFERENCES %s(id_%s),\n", relation_attributes[i].table_1, relation_attributes[i].table_1, relation_attributes[i].table_1);
        fprintf(output_file, "    FOREIGN KEY (id_%s) REFERENCES %s(id_%s)\n", relation_attributes[i].table_2, relation_attributes[i].table_2, relation_attributes[i].table_2);
        fprintf(output_file, ");\n\n");
    }

    for (xmlNodePtr node = root->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            insertData(output_file, node, 0, NULL);
        }
    }
    fclose(output_file);

    printf("Generated SQL file: %s\n", output_path);
}

void parseXml(const char *filename, const char *output_path) {
    xmlDocPtr doc = xmlReadFile(filename, NULL, 0);
    if (doc == NULL) {
        fprintf(stderr, "Unable to read the file: %s\n", filename);
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        fprintf(stderr, "Error: the XML is empty.\n");
        xmlFreeDoc(doc);
        return;
    }

    printf("Root element: %s\n", root->name);
    generateSql(root, output_path);

    xmlFreeDoc(doc);
    xmlCleanupParser();
}
