#ifndef SQL_PREPARE_PARSER_H
#define SQL_PREPARE_PARSER_H

#ifndef MAX_VALUE_LEN
#define MAX_VALUE_LEN  512
#endif

/*
 * Currently, we support follow data types:
 *   DB_UIN: interger
 *   DB_NUM: double
 *   DB_STR: character
 */
typedef enum data_type_t {
    DB_UIN,
    DB_NUM,
    DB_STR
} data_type_t;

/* Data list for every SQL's binding parameters */
typedef struct sql_data_t {
    int          index;
    data_type_t  type;
    union {
        unsigned int    data_num;
        double          data_double;
        char            data_str[MAX_VALUE_LEN];
    } data;
    struct sql_data_t  *next;
} sql_data_t;

int sql_translate(char *, char *, sql_data_t **);

#endif

