#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define TRACKER_RANK 0
#define MAX_FILES 10
#define FILE_SIZE 11 // * in_10.txt, va avea maxim(in cazul temei) 2 cifre rank-ul
#define BUFF_SIZE 64
#define nullptr NULL

// * Macros
#define MAX(a, b) ((a > b) ? a : b)

// * Mpi TAGS
#define HASH_TAG 0
#define CLIENT_TYPE_TAG 1
#define ACK_TAG 2
#define PEERS_SEEDERS_TRANSFER_TAG 3
#define REQUEST_TAG 4
#define INFORM_TAG 5

// * Debug defines
#define SHOW_TRACKER_DEBUG 0
#define SHOW_RECV_SWARM_DEBUG 0
#define SHOW_DOWNLOAD_DEBUG 0

typedef enum CLIENT_TYPE{
    SEEDER = 0,
    PEER,
    LEECHER
}CLIENT_TYPE;

typedef struct file_segment{
    char hash[HASH_SIZE + 1];
}file_segment;

typedef struct peer_info{
    int file_id; // * practic swarmul fisierului file<file_id>
    int peer_rank;
    size_t segment_count;
    file_segment segments[MAX_CHUNKS];
}peer_info;

typedef struct peers_list{
    peer_info* peers_array; // * peers/seeders
    int peers_count;
}peers_list;

typedef struct f_name{
    char file_name[FILE_SIZE];
}f_name;

typedef struct file_data{
    char file_name[FILE_SIZE];
    int file_id; // * numarul fisierului, file<id>
    size_t segment_count;
    file_segment segments[MAX_CHUNKS];
}file_data;

typedef struct client_files{
    int client_rank;
    size_t owned_files_count;
    file_data* owned_files;
    size_t wanted_files_count;
    f_name* wanted_files;
    peers_list* peers;
    CLIENT_TYPE client_type;
}client_files;

// * Tracker
// * swarm-ul fisierului = toti clientii care detin o parte din acel fisier
// * swarm[0] - ce clienti detin o parte din file1 and so on..
typedef struct swarm{
    char file_name[FILE_SIZE];
    int* clients_in_swarm; 
    int clients_in_swarm_count; 
}swarm;

// * tracker_data[0] - Clientul 1 and so on..
typedef struct tracker_data{
    int rank; // * rank of the client
    size_t files_count;
    file_data* files; // * the files that the client has/owns
    CLIENT_TYPE client_type;
}tracker_data;

typedef struct tracker_ds {
    int client_count;
    tracker_data* data;
    swarm* swarms; // * swarms for each file
    int swarm_size;
}tracker_ds;

// * Init the client
void read_from_file(client_files* client, int rank){
    FILE* f_ptr;
    char file_name[sizeof("in_10.txt") + 1];
    sprintf(file_name, "%s%d.txt", "in", rank);
    char buffer[BUFF_SIZE];

    f_ptr = fopen(file_name, "r");

    fgets(buffer, BUFF_SIZE, f_ptr);
    client->owned_files_count = atoi(buffer);
    client->client_rank = rank;

    if(!client->owned_files_count) {
        client->owned_files = nullptr;
    } else {
        client->owned_files = (file_data*)malloc(sizeof(file_data) * client->owned_files_count);
        
        for(size_t i = 0; i < client->owned_files_count; ++i){
            fgets(buffer, BUFF_SIZE, f_ptr);
            char* file_name = strtok(buffer, " ");
            strcpy(client->owned_files[i].file_name, file_name);
            client->owned_files[i].file_id = atoi(&file_name[strlen(file_name) - 1]);

            size_t segment_count = atoi(strtok(NULL, "\n"));
            client->owned_files[i].segment_count = segment_count;

            for(size_t j = 0; j < segment_count; ++j){
                fgets(buffer, BUFF_SIZE, f_ptr);
                strncpy(client->owned_files[i].segments[j].hash, buffer, HASH_SIZE);
                client->owned_files[i].segments[j].hash[HASH_SIZE] = '\0';
            }
        }
    }

    fgets(buffer, BUFF_SIZE, f_ptr);
    client->wanted_files_count = atoi(buffer);

    if(!client->wanted_files_count){
        client->wanted_files = nullptr;
    } else {
        client->wanted_files = (f_name*)malloc(sizeof(f_name) * client->wanted_files_count);
        for(size_t i = 0; i < client->wanted_files_count; ++i){
            fgets(buffer, BUFF_SIZE, f_ptr);

            // * Se afisau dubios fisierele, righ-trim pe newline
            if(buffer[strlen(buffer) - 1] == '\n'){
                buffer[strlen(buffer) - 1] = '\0';
            }

            strncpy(client->wanted_files[i].file_name, buffer, sizeof(client->wanted_files[i].file_name) - 1);
            client->wanted_files[i].file_name[sizeof(client->wanted_files[i].file_name) - 1] = '\0';
        }
    }

    // * Nu stie de niciun client din retea
    client->peers = (peers_list*)calloc(client->wanted_files_count, sizeof(peers_list));

    // * Vedem ce tip de Client este
    if(client->owned_files && !client->wanted_files)
        client->client_type = SEEDER; // * 0
    else if(client->owned_files && client->wanted_files)
        client->client_type = PEER; // * 1
    else
        client->client_type = LEECHER; // * 2

    fclose(f_ptr);
}

/*
    * Prin a detine/own intelegem macar sa aibe fisierul.
    * Fie ca e gol sau nu
*/
bool file_is_owned(client_files* client, int file_id){
    if(!client->owned_files_count)
        return false;

    for(size_t i = 0; i < client->owned_files_count; i++){
        if(client->owned_files[i].file_id == file_id){
            return true;
        }
    }

    return false;
}

void add_file_to_owned(client_files* client, int file_id) {
    /*
        * Daca nu exista, doreste sa descarce un fisier pe care nu il
        * detine, dar il va detine
    */
    if(!client->owned_files){
        client->owned_files_count++;
        client->owned_files = (file_data*)malloc(sizeof(file_data));
    }else{
        client->owned_files_count++;
        client->owned_files = (file_data*)realloc(client->owned_files, sizeof(file_data) * client->owned_files_count);
    }

    // * Initializam noul fișier
    file_data* new_file = &client->owned_files[client->owned_files_count - 1];
    sprintf(new_file->file_name, "file%d", file_id);
    new_file->file_id = file_id;
    new_file->segment_count = 0;
}

bool has_segment(const file_data *data, const file_segment seg) {
    if(!data)
        return false;

    for(size_t i = 0; i < data->segment_count; ++i){
        if(strcmp(data->segments[i].hash, seg.hash) == 0){
            return true;
        }
    }

    return false;
}

bool add_segment_to_file_data(file_data *data, const file_segment seg) {
    if (!data)
        return false;

    // * In orice caz, chiar daca nu vom intra aici vreodata
    if(data->segment_count >= MAX_CHUNKS){
        return false;
    }

    strncpy(data->segments[data->segment_count].hash, seg.hash, HASH_SIZE);
    data->segment_count++;

    return true;
}

file_data* find_file_data(file_data* f_data, size_t search_count, int file_id) {
    if(!f_data) 
        return nullptr;

    // * Cautam un fisier existent cu file_id
    for(size_t i = 0; i < search_count; ++i){
        if(f_data[i].file_id == file_id){
            return &f_data[i];
        }
    }

    return nullptr;
}

// * for the tracker
void create_file_swarms(tracker_ds* m_tracker, int numtasks){
    m_tracker->swarms = (swarm*)malloc(sizeof(swarm) * m_tracker->swarm_size);
    swarm* m_swarm = m_tracker->swarms;
    
    for(int i = 0; i < m_tracker->swarm_size; ++i) {
        sprintf(m_swarm[i].file_name, "file%d", i + 1);
        m_swarm[i].clients_in_swarm = nullptr;
        m_swarm[i].clients_in_swarm_count = 0;
    }

    for(int rank = 1; rank < numtasks; ++rank) {
        tracker_data* client_data = &m_tracker->data[rank - 1];

        for (int i = 0; i < client_data->files_count; ++i) {
            char* file_name = client_data->files[i].file_name;
            // Numarul fisierului
            int file_id = atoi(&file_name[strlen(file_name) - 1]) - 1;

            if (file_id >= 0 && file_id < m_tracker->swarm_size) {
                swarm* current_swarm = &m_swarm[file_id];
                int* curr_swarm_count = &current_swarm->clients_in_swarm_count;
                // TODO: AM SCHIMBAT DIN rank - 1 IN RANK, vezi DACA E CV STRICAT
                if(!current_swarm->clients_in_swarm){
                    current_swarm->clients_in_swarm = (int*)malloc(sizeof(int));
                    current_swarm->clients_in_swarm[*curr_swarm_count] = rank;
                    (*curr_swarm_count) = 1;
                }else{
                    // Realloc
                    int* temp = (int*)realloc(current_swarm->clients_in_swarm, (*curr_swarm_count + 1) * sizeof(int));
                    if(!temp) {
                        printf("COULDN'T REALLOCATE SWARM %d", file_id);
                        return;
                    } else {
                        current_swarm->clients_in_swarm = temp;
                        current_swarm->clients_in_swarm[*curr_swarm_count] = rank;
                        (*curr_swarm_count)++;
                    }
                }
            }
        }
    }
}

void write_to_file(const char* file_name, file_data* data){
    FILE* f_out = fopen(file_name, "w");
    
    for(size_t j = 0; j < data->segment_count; ++j){
        fprintf(f_out, "%s\n", data->segments[j].hash);
        /*
            * Daca nu dadeam flush, doar pentru testul1 ultimul fisier(file3)
            * pentru clientul 3 nu era niciodata scris, programul intra intr-o blucla infinita.
            * Daca comentam doar functia fprintf, totul functiona normal. Dupa
            * ce am adauga si fflush, totul functioneaza cum trebuie.
        */
        fflush(f_out);
    }

    fclose(f_out);  
}

bool tracker_client_has_file(tracker_ds* m_tracker, int file_id, int rank){
    if(!m_tracker->data[rank].files_count)
        return false;
    
    for(size_t i = 0; i < m_tracker->data[rank].files_count; ++i){
        if(m_tracker->data[rank].files[i].file_id == file_id)
            return true;
    }

    return false;
}

void tracker_add_file_to_owned(tracker_ds* m_tracker, int file_id, int rank){
    size_t files_count = 0;
    if(!m_tracker->data[rank].files){
        files_count = ++m_tracker->data[rank].files_count;
        m_tracker->data[rank].files = (file_data*)malloc(sizeof(file_data));
    }else{
        files_count = ++m_tracker->data[rank].files_count;
        m_tracker->data[rank].files = (file_data*)realloc(m_tracker->data[rank].files, sizeof(file_data) * files_count);
    }

    assert(files_count > 0);

    file_data* new_file = &m_tracker->data[rank].files[files_count - 1];
    sprintf(new_file->file_name, "file%d", file_id);
    new_file->file_id = file_id;
    new_file->segment_count = 0;
}

// * DEBUG/LOG
void show_tracker(tracker_ds* m_tracker, int numtasks){
    printf("clients: %d\n", m_tracker->client_count);
    tracker_data* data = m_tracker->data;

    for(int i = 0; i < m_tracker->client_count; ++i){
        if(!data[i].files){
            printf("Rank %d doesn't own any files\n", data[i].rank);
            continue;
        }

        printf("Rank %d has: \n", data[i].rank);
        printf("Files: ");
        for(int j = 0; j < data[i].files_count; ++j){
            printf("%s\n", data[i].files[j].file_name);
            for(int k = 0; k < data[i].files[j].segment_count; ++k){
                printf("%s\n", data[i].files[j].segments[k].hash);
            }
        }

        printf("\n");
    }

    // Show swarm data
    swarm* m_swarm = m_tracker->swarms;
    for(int rank = 0; rank < m_tracker->swarm_size; ++rank){
        if(!m_swarm[rank].clients_in_swarm_count){
            printf("Swarm-ul %d nu are niciun fisier.\n", rank + 1);
            continue;
        }

        printf("Dimensiunea swarm-ului %d: %d\n", rank + 1, m_swarm[rank].clients_in_swarm_count);
        for(int i = 0; i < m_swarm[rank].clients_in_swarm_count; ++i){
            printf("Clientul %d ", m_swarm[rank].clients_in_swarm[i]);
        }

        printf("\n");
    }  

    for(int rank = 0; rank < m_tracker->client_count; ++rank){
        printf("Tipul clientului %d: %d\n", rank + 1, m_tracker->data[rank].client_type);
    }
}

// * free helper, aparent trebuie sa fie statica daca nu apartine aceluiasi translation unit
static inline void free_and_point_to_null(void** m_ptr){
    free(*m_ptr);
    (*m_ptr) = nullptr;
}

void free_tracker(tracker_ds* m_tracker){
    tracker_data** data = &m_tracker->data;
    
    for(int i = 0; i < m_tracker->client_count; ++i){
        free_and_point_to_null((void**)&(*data)[i].files);
    }

    free_and_point_to_null((void**)&(*data)->files);
    free_and_point_to_null((void**)data);
}

void free_client_files(client_files* cf){
    free_and_point_to_null((void**)&cf->owned_files);
    free_and_point_to_null((void**)&cf->wanted_files);
    
    // * Free peers
    for(size_t i = 0; i < cf->wanted_files_count; ++i){
        free_and_point_to_null((void**)&cf->peers[i].peers_array);
    }

    free_and_point_to_null((void**)&cf->peers);
}

#endif