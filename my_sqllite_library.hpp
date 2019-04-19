#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdbool.h>

using namespace std;

#ifndef __MY_SQLITE_LIB_HPP
#define MY_SQLITE_LIB_HPP

//specificare il percorso
#define DB_FOLDER "db/"
#define HTML_FOLDER "html/"
//speccificare il nome del file temporaneo che verrà composto
#define TMP_FILE_NAME "html/temp.html"
#define MAKE_TABLE_TMP_FILE_NAME "make_table_callback_temp_file.txt"

/*
  cerca un carattere sql, successivamente accetta un numero indefinito di spazi
  che possono esserci come non esserci (\s*)
  poi cerca la parola chiave 'database=', dopo di essa sono accettati un numero
  indefinito di spazii che possono esserci come non esserci;
  nelle parentesi tonde c'è ciò che desideriamo intercettare: il nome del Database
  l'espressione [^ ]* significa "accetta qualsiasi carattere che non sia uno spazio"
  in modo da evitarci problemi di spazi dopo il nome del db; accetto successivamente un
  numero indefinito si spazi fino al /> che chiude il tag
*/
#define FIND_TAG_DB_REGEX "<sql\\s*database=\\s*([^ ]*)\\s*\\/>"
//regex che cerca una query nel rispettivo tag <sql/>
#define FIND_TAG_QUERY_REGEX "<sql\\s*query=\\s*(.*;)\\s*\\/>"
/*Controlla la presenza di tag sql*/
#define CHECK_MORE_SQL_TAG "<sql(.*)\\/>"
//definizione dei tag per scrivere la tabella html
#define TAG_TABLE_START "<table border=\"1px solid\" align=\"center\">\n\t"
#define TAG_TABLE_END "</table>\n"
#define TAG_TR_START "<tr>\n\t"
#define TAG_TR_END "</tr>\n"
#define TAG_TH_START "<th>\n\t"
#define TAG_TH_END "</th>\n"
#define TAG_TD_START "<td>\n\t"
#define TAG_TD_END "</td>\n"


#define MAX_ERROR_MSG 0x1000

//struttura per crezione tabella
typedef struct {

  //stringa con query
  char* query;
  //posizione della query nel file originale
  int query_index;
  //nome del file da utilizzare
  char* file_path;

}TABLE_INFO;

class My_sqlite_lib{


    protected:
                //Definiamo la struttura per salvarci le informazioni per la tabella globale
                static TABLE_INFO* info_table;
                /*
                  mi serve per permettere alla funzione di callback di scrivere solo una volta
                  sul file temporaneo il nome delle colonne tel db
                */
                static bool is_row_index_writed;

    public:
            //indicare il percorso del file da analizzare
            My_sqlite_lib(char*);
            char* build_page();
            ~My_sqlite_lib();


    private:
            //Compila la regex
            int compile_regex (regex_t *, const char *);
            //Stampa a terminale il risultato di una query
            void execute_query(sqlite3*, char*);
            //Utilizzata per interrogare il db
            static int callback(void *, int, char **, char **);
            /*
              Utilizzata quando non ci si aspetta un valore di ritorno da una query,
              come una CREATE TABLE
            */
            int null_object();
            /*
               Analizza il file che si trova nel percorso passato, trova il tag sql con il
               nome del db, rimuove la riga con il tag e restituisce il nome del db;
               utilizza le regex per la ricerca
            */
            char* get_db_name(char*);
            /*
              Stesso compito della funzione precedente, ma questa compila una specifica
              struttura che ci permetterà successivamente di conoscere la posizione del
              tag sql all'interno del file per andare a scrivere la tabella html con i
              valori restituiti dall'interrogazione
            */
            void find_query(char*);
            //Costruisce la tabella dopo aver interrogato il database
            void make_table(sqlite3*);
            /*
              Funzione chiamata da quella che esegue la query che viene fatta nella funzione
              precedente, (utilizza TABLE_INFO)
            */
            static int make_table_callback(void*, int, char **, char **);
            //Restituisce la grandezza del file che si trova nel percorso passatogli
            int get_file_size(char*);
            //Rimove (dal file corrsipondente al path passato) il range specificato
            void remove_range_from_file(char*, int, int);
            /*Controlla la presenza di tag sql, mi serve per continuare a
              creare tabelle finchè rimangono interrogazioni*/
            bool more_tag();

};

My_sqlite_lib::My_sqlite_lib(char* path){
  //concateno il nome del file passatoci con il percorso
  char* file_name = strcat(strdup(HTML_FOLDER), path);
  //creo il primo file
  FILE* original_file = fopen(file_name, "r");

  if (original_file == NULL){
    printf("Attenzione, il file indicato non esiste\n");
  }

  //dopo aver'verificato che nel primo ci sia qualcosa apro il secondo
  FILE* temp_file = fopen(strdup(TMP_FILE_NAME), "w");
  char ch;

  if (temp_file == NULL){
    printf("Errore nella creazione del file temporaneo\n");
    exit(EXIT_FAILURE);
  }

  while ((ch = fgetc(original_file)) != EOF)
      fputc(ch, temp_file);

  //non avendone più bisogno, chiudo sia il file originale
  fclose(original_file);
  //che il temporaneo, spetterà alle funzioni cambiarlo
  fclose(temp_file);

  //Alloco lo spazio in memoria per variabile globale
  info_table = (TABLE_INFO*) malloc(sizeof(TABLE_INFO));
  //Inserisco già il nome del file su cui lavorare nella Struttura
  info_table->file_path = strdup(TMP_FILE_NAME);
  //dato che non ho ancora scritto niente metto a false la variabile globale di controllo
  is_row_index_writed = false;

}

char* My_sqlite_lib::build_page(){

  //Prelevo i nomi del db concatenandone il nome al percorso
  //Attenzione allo strdup
  char* db_name = strcat(strdup(DB_FOLDER), strdup(info_table->file_path));

  sqlite3* my_db;

  //Apertura del db e gestione eventuali errori
  if (sqlite3_open(db_name, &my_db)){
    printf("Impossibile accedere al database: %s\n", sqlite3_errmsg(my_db));
    exit(EXIT_FAILURE);
  }else
    printf("Database aperto con successo\n");

  int ret;
  char* error_message = 0;

  /*Fino a che rimangono tag sql avvio il processo di ricerca, cancellazione
    tag e aggiunta tabella all'interno del file*/
  while (more_tag()){
    find_query(strdup(TMP_FILE_NAME));
    make_table(my_db);
  }

  sqlite3_free(error_message);
  sqlite3_close(my_db);

}

/*
  Funzione che facilita la compilazione di una regex
  gestisce gli errori e ritorna un valore
*/
int My_sqlite_lib::compile_regex (regex_t * r, const char * regex_text){

    int status = regcomp(r, regex_text, REG_EXTENDED|REG_NEWLINE);

    if (status != 0) {
	    char error_message[MAX_ERROR_MSG];
	    regerror (status, r, error_message, MAX_ERROR_MSG);
      printf ("Regex error compiling '%s': %s\n",
              regex_text, error_message);
            return -1;
    }

    return 0;

  }

  /*
    L'idea sarebbe quella di intercettare la stringa contentente il nome
    del db a cui connetterci e successivamente eliminare la riga con il
    corrispondente <sql> dal file temporaneo che abbiamo creato

  */

char* My_sqlite_lib::get_db_name(char* file_path){

    FILE* file = fopen(file_path, "r");
    //buffer per lettura sequenziale del file
    int file_size = get_file_size(file_path);
    char buffer[file_size + 1];

    //Copio il file in un buffer
    fread(buffer, file_size, sizeof(char), file);

    //chiudo il file
    fclose(file);

    //Passiamo adesso alla ricerca del nome del database

    //regex in cui verrà compilato il comando
    regex_t regex;
    //il buffer lo preferisco come puntatore
    char* file_content = strdup(buffer);;
    //comando contenente la regex in formato stringa

    //procediamo alla compilazione della regex
    compile_regex(&regex, strdup(FIND_TAG_DB_REGEX));

    //numero dei matches che consentiamo di trovare
    int n_matches = 10;
    /*
      vettore con i matches effettivi, struttura:

      regmatch_t{

        int rm_so; puntatore all'inizio dell'occorrenza
        int rm_eo; puntatore alla fine dell'occorrenza
      }

    */
    regmatch_t matches_array[n_matches];
    //eseguiamo la regex
    regexec(&regex, file_content, n_matches, matches_array, 0);

    //ottengo il nome del db dal secondo gruppo
    char *result;
    //alloco result
    result = (char*)malloc(matches_array[1].rm_eo - matches_array[1].rm_so);
    //copio, partendo dalla posizione del buffer interessata, la lunghezza del nome del db
    strncpy(result, &buffer[matches_array[1].rm_so], matches_array[1].rm_eo - matches_array[1].rm_so);
    char* db_name = strdup(result);
    //deallochiamo la stringa di cortesia
    free(result);

    //provvediamo adesso a rimuovere la linea dal file
    remove_range_from_file(file_path, matches_array[0].rm_so, matches_array[0].rm_eo);

    return db_name;
  }

void My_sqlite_lib::find_query(char* file_path){

  FILE* file = fopen(file_path, "r");
  //buffer per lettura sequenziale del file
  int file_size = get_file_size(file_path);
  char buffer[file_size + 1];
  //Copio il file in un buffer
  fread(buffer, file_size, sizeof(char), file);
  //chiudo il file
  fclose(file);

  //Passiamo adesso alla ricerca della query

  //regex in cui verrà compilato il comando
  regex_t regex;
  //il buffer lo preferisco come puntatore
  char* file_content = strdup(buffer);

  //procediamo alla compilazione della regex
  compile_regex(&regex, strdup(FIND_TAG_QUERY_REGEX));
  int n_matches = 10;

  regmatch_t matches_array[n_matches];
  //eseguiamo la regex
  regexec(&regex, file_content, n_matches, matches_array, 0);

  //ottengo il nome del db dal secondo gruppo
  char *result;
  //alloco result
  result = (char*)malloc(matches_array[1].rm_eo - matches_array[1].rm_so);
  //copio, partendo dalla posizione del buffer interessata, la lunghezza del nome del db
  strncpy(result, &buffer[matches_array[1].rm_so], matches_array[1].rm_eo - matches_array[1].rm_so);
  char* query_name = strdup(result);
  //deallochiamo la stringa di cortesia
  free(result);
  //provvediamo adesso a rimuovere la linea dal file
  remove_range_from_file(file_path, matches_array[0].rm_so, matches_array[0].rm_eo);
  //compiliamo la struttura e restituiamola
  info_table->query = strdup(query_name);
  info_table->query_index = matches_array[0].rm_so;
  info_table->file_path = file_path;


}

void My_sqlite_lib::make_table(sqlite3* my_db){

  char* error_message;
  char* query = strdup(info_table->query);

  //Creaiamo il file temporaneo per il buffer
  FILE* temp_file_1 = fopen(MAKE_TABLE_TMP_FILE_NAME, "w");
  //Scriviamo la prima riga
  fputs(strdup(TAG_TABLE_START), temp_file_1);
  fclose(temp_file_1);

  int ret = sqlite3_exec(my_db, query, make_table_callback, 0, &error_message);

  if( ret != SQLITE_OK ){
      printf("Errore durante l'interrogazione: %s\n", error_message);
      sqlite3_free(error_message);
   }else
      sqlite3_free(error_message);

    //Riapro il file in "a" per non sovrascrivere tutto
    FILE* temp_file_2 = fopen(MAKE_TABLE_TMP_FILE_NAME, "a");
    //chiudo la tabella
    fputs(TAG_TABLE_END, temp_file_2);
    fclose(temp_file_2);
    //rimetto la variabile di controllo a false per le chiamate successive
    is_row_index_writed = false;

    //Procedo adesso a leggere tutto ciò che ho scritto in un buffer
    FILE* to_read = fopen(MAKE_TABLE_TMP_FILE_NAME, "r");
    int size_of_to_read_file = get_file_size(strdup(MAKE_TABLE_TMP_FILE_NAME));
    char buffer[size_of_to_read_file];
    fread(buffer, size_of_to_read_file, sizeof(char), to_read);
    fclose(to_read);
    remove(MAKE_TABLE_TMP_FILE_NAME);
    printf("Buffer letto: %s\n", buffer);

    //creo il buffer finale
    //DEBUG printf("lunghezza del file finale: %d\n",(int) strlen(buffer) + get_file_size(info_table->file_path));
    int final_buffer_lenght = (int) strlen(buffer) + get_file_size(info_table->file_path);
    char final_buffer[final_buffer_lenght];
    int sql_index = info_table->query_index;
    //Procedo adesso a scrivere la tabella sul file originale
    printf("query index %d\n", sql_index);
    FILE* temp_file_3 = fopen(info_table->file_path, "r");

      for (int i = 0; i < final_buffer_lenght; i++){

        //se arrivo al punto in cui c'era la query aggiungo la tabella
        if (i == sql_index){
          for(int j = 0; j < strlen(buffer); j++){
            final_buffer[i] = buffer[j];
            i++;
          }
        }
        final_buffer[i] = (char) fgetc(temp_file_3);
        //printf("buffer[%d] = %c\n", i, final_buffer[i]);
      }

    fclose(temp_file_3);
    printf("Final file: %s\n", final_buffer);
    //sovrascrivo il vecchi file con la tabella
    FILE* final_file = fopen(info_table->file_path, "w");
    fwrite(final_buffer, 1, final_buffer_lenght, final_file);
    fclose(final_file);

}

int My_sqlite_lib::make_table_callback (void *query_result, int cells_number, char **rows, char **rows_index){

  int sql_index = info_table->query_index;

  /*
    La strategia per la creazione di una tabella con i risultati sql è la seguente:
    mi appoggio su un file per scrivere il codice html con all'interno dei tag
    le informazioni estrapolate dalla query, successivamente salvo il contenuto
    del file creato in un buffer e lo elimino.
    Vado quindi a instanziare un buffer grande come il file in cui aggiungere la
    tabela + la grandezza del buffer in cui abbiamo costruito la tabella;
    Sovrascrivo quindi il file originale carattere per carattere, quando arrivo
    al punto in cui si trovava il tag <sql\> con la query (lo ricavo dalla struttura
    globale info_query->query_index) vado ad inserire il contenuto, sempre carattere
    per carattere, del buffer creato
  */

  FILE* temp_file = fopen(MAKE_TABLE_TMP_FILE_NAME, "a");
  //aggiungo una riga alla tabella
  fputs(TAG_TR_START, temp_file);
  //Aggiungo al file prima i nomi della colonne (controllo la variabile globale)
  if(!is_row_index_writed){
    for(int i = 0; i < cells_number; i++) {
       fputs(TAG_TH_START, temp_file);
       fputs(rows_index[i], temp_file);
       fputs("\n", temp_file);
       fputs(TAG_TH_END, temp_file);
    }
    is_row_index_writed = true;
  }
  //chiudo la riga alla tabella
  fputs(TAG_TR_END, temp_file);
  //apro la riga dei contenuti
  fputs(TAG_TR_START, temp_file);
  for(int i = 0; i < cells_number; i++) {
     fputs(TAG_TD_START, temp_file);
     //Prima di inserire un qualche valore controllo che non sia null
     if (rows[i] != NULL)
      fputs(rows[i], temp_file);
     else
      fputs("NULL", temp_file);

     fputs("\n", temp_file);
     fputs(TAG_TD_END, temp_file);
  }
  //chiudo la riga alla tabella
  fputs(TAG_TR_END, temp_file);
  fclose(temp_file);
  return 0;

}

int My_sqlite_lib::callback(void *query_result, int cells_number, char **rows, char **rows_index) {

   for(int i = 0; i < cells_number; i++) {
      //se nella cella è presente un dato lo stampa, altrimenti inserisce NULL
      printf("%s: %s\n", rows_index[i], rows[i] ? rows[i] : "NULL");
   }
   printf("\n");
   return 0;
}

int My_sqlite_lib::null_object(){  return 0;  }

void My_sqlite_lib::execute_query(sqlite3* my_db, char* sql){

  char* error_message = 0;
  int ret = sqlite3_exec(my_db, sql, callback, 0, &error_message);

  if( ret != SQLITE_OK ){
      printf("Errore durante l'interrogazione: %s\n", error_message);
      sqlite3_free(error_message);
   } else

  sqlite3_free(error_message);

}

int My_sqlite_lib::get_file_size(char* file_path){

  //apro il file in modalità lettura
  FILE* file = fopen(file_path, "r");

  //controlliamo che il file passato dall'utente esista effettivamente
  if (file == NULL) {
    printf("File Not Found!\n");
    return -1;
  }

    /*
    per utilizzo di questa fuzione vedre es calcolare_dimesione_file
    nella cartella esercizi
    */

    fseek(file, 0L, SEEK_END);

    // calcoliamo la grandezza del file passato mediante ftell()
    int file_size = ftell(file);

    // chiudo il file
    fclose(file);

    return file_size;

}

/*

  Questa funzione si occupa di rimuovere la sezione di file in cui abbiamo trovato un
  ta sql, parametri:

  char* path:   Percorso del file da cui rimuovere la sezione
  int start:    Inizio della sezione da rimuovere
  int end:      Fine della sezione da rimuovere

*/
void My_sqlite_lib::remove_range_from_file(char* path, int start, int end){

  //prima copio il file in un buffer:
  int file_size = get_file_size(path);
  char buffer[file_size];
  FILE* to_close = fopen(path, "r");
  fread(buffer, file_size, sizeof(char), to_close);
  fclose(to_close);
  FILE* file = fopen(path, "w");
  for(int i = 0; i < file_size; i++){
    if(i < start || i > end)
      fputc(buffer[i], file);
  }
  fclose(file);
}

bool My_sqlite_lib::more_tag(){

  regex_t regex;
  //procediamo alla compilazione della regex
  compile_regex(&regex, strdup(CHECK_MORE_SQL_TAG));
  int fs = get_file_size(strdup(info_table->file_path));
  char file_content[fs];
  FILE* file = fopen(strdup(info_table->file_path), "r");
  fread(file_content, fs, sizeof(char), file);
  fclose(file);

  //numero dei matches che consentiamo di trovare
  int n_matches = 10;
  regmatch_t matches_array[n_matches];
  //eseguiamo la regex
  regexec(&regex, file_content, n_matches, matches_array, 0);

  /*Controllo la presenza di un eventuale match e restituisco il valore opportuno*/
  if (matches_array == NULL)
    return false;
  else
    return true;

}




#endif
