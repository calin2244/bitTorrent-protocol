#include "mpi/mpi.h"
#include "./utils.h"
#include <pthread.h>

void send_data_to_tracker(client_files* client){
    MPI_Send(&client->owned_files_count, 1, MPI_INT, TRACKER_RANK, HASH_TAG, MPI_COMM_WORLD);

    MPI_Send(&client->client_type, 1, MPI_INT, TRACKER_RANK, CLIENT_TYPE_TAG, MPI_COMM_WORLD);

    /*
        * char file_name[FILE_SIZE];
        * size_t segment_count;
        * file_segment segments[MAX_CHUNKS];
    */
    for(size_t i = 0; i < client->owned_files_count; ++i) {
        MPI_Send(client->owned_files[i].file_name, FILE_SIZE, MPI_CHAR, TRACKER_RANK, HASH_TAG, MPI_COMM_WORLD);

        size_t segment_count = client->owned_files[i].segment_count;
        MPI_Send(&segment_count, 1, MPI_UNSIGNED, TRACKER_RANK, HASH_TAG, MPI_COMM_WORLD);

        for(size_t j = 0; j < segment_count; ++j) {
            MPI_Send(client->owned_files[i].segments[j].hash, HASH_SIZE, MPI_CHAR, TRACKER_RANK, HASH_TAG, MPI_COMM_WORLD);
        }
    }    
}

void receive_data_from_clients(tracker_ds* m_tracker, int numtasks) {
    m_tracker->client_count = numtasks - 1;
    m_tracker->data = (tracker_data*)malloc(sizeof(tracker_data) * m_tracker->client_count);
    int number_of_files = 0; // for swarm

    for(int rank = 1; rank < numtasks; ++rank) {
        int owned_files_count;

        // * Primim numarul de fisiere detinute de la client
        MPI_Recv(&owned_files_count, 1, MPI_INT, rank, HASH_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        m_tracker->data[rank - 1].files_count = owned_files_count;
        m_tracker->data[rank - 1].rank = rank;

        // * Primim tipul clientului
        MPI_Recv(&m_tracker->data[rank-1].client_type, 1, MPI_INT, rank, CLIENT_TYPE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if(!owned_files_count){
            m_tracker->data[rank - 1].files = nullptr;
            continue;
        }

        m_tracker->data[rank - 1].files = (file_data*)malloc(sizeof(file_data) * owned_files_count);

        for(int j = 0; j < owned_files_count; ++j) {
            file_data f_data;
            memset(&f_data, 0, sizeof(file_data));

            // * Primim numele fisierului
            MPI_Recv(f_data.file_name, FILE_SIZE, MPI_CHAR, rank, HASH_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            strcpy(m_tracker->data[rank - 1].files[j].file_name, f_data.file_name);

            // * Id-ul fisierului (Numarul de identificare al fisierului)
            m_tracker->data[rank - 1].files[j].file_id = atoi(&f_data.file_name[strlen(f_data.file_name) - 1]);

            char* file_digit = &f_data.file_name[strlen(f_data.file_name) - 1];
            number_of_files = MAX(number_of_files, atoi(file_digit));

            // * Primim numarul de segmente
            MPI_Recv(&f_data.segment_count, 1, MPI_UNSIGNED, rank, HASH_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            m_tracker->data[rank - 1].files[j].segment_count = f_data.segment_count;

            // * Primim hash-ul fiecarui segment
            for(int k = 0; k < f_data.segment_count; ++k) {
                MPI_Recv(f_data.segments[k].hash, HASH_SIZE, MPI_CHAR, rank, HASH_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                strcpy(m_tracker->data[rank - 1].files[j].segments[k].hash, f_data.segments[k].hash); 
            }
        }
    }

    // * Create the swarms >:)
    /*
        * char file_name[FILE_SIZE];
        * int* ranks_in_swarm; 
        * int clients_in_swarm_count; 
    */

   /*
        * Chiar daca multe elemente din swarm vor fi alocate degeaba,
        * asa a fost cel mai convenabil de creeat si accesat swarm-ul fisierelor.
   */
    m_tracker->swarm_size = number_of_files;
    if(!number_of_files){
        m_tracker->swarms = nullptr;
        return;
    }

    create_file_swarms(m_tracker, numtasks);
    
    // * Poate incepe comuncarea intre clienti
    for(int rank = 1; rank < numtasks; ++rank){
        MPI_Send("OK", 2, MPI_CHAR, rank, ACK_TAG, MPI_COMM_WORLD);
    }
}

void request_seeders_peers_list(client_files* client){
    CLIENT_TYPE client_type = client->client_type;
    size_t wanted_files_count = client->wanted_files_count;
    f_name* wanted_files = client->wanted_files;

    // * Trimitem tipul clientului catre tracker, sa stim cate receiv-uri si send-uri se vor
    // * face pentru fiecare caz
    MPI_Send(&client_type, 1, MPI_INT, TRACKER_RANK, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD); 
    
    switch(client_type){
        case SEEDER: {
            // nu doreste nimic, nu trimitem nimic
            break;
        }
        // Leecher-ii si peer-ii au aceeasi logica
        case PEER: 
        case LEECHER: {
            MPI_Send(&wanted_files_count, 1, MPI_UNSIGNED, TRACKER_RANK, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD); 
            int files_id[wanted_files_count];

            for(size_t i = 0; i < wanted_files_count; ++i){
                const char* f_name = wanted_files[i].file_name;
                int id = atoi(&f_name[strlen(f_name) - 1]);
                files_id[i] = id;
            }

            MPI_Send(files_id, wanted_files_count, MPI_INT, TRACKER_RANK, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD);
            break;
        }
        default:
            break;
    }

    // * Primim seeders/peers in ordinea file-urile din wanted files
    MPI_Status status;
    for(int i = 0; i < wanted_files_count; ++i){
        
        char* file_name = client->wanted_files[i].file_name;
        int in_swarm_count = -1;
        MPI_Recv(&in_swarm_count, 1, MPI_INT, TRACKER_RANK, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD, &status);
        int* ranks_in_swarm = (int*)malloc(sizeof(int) * in_swarm_count);
        MPI_Recv(ranks_in_swarm, in_swarm_count, MPI_INT, TRACKER_RANK, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD, &status);
        
        // * Primim segmentele dorite
        size_t segment_count = 0;
        file_segment segments[MAX_CHUNKS];
        // * de la 0...wanted_files_count
        peers_list* peers_arr = client->peers;
        peers_arr[i].peers_count = in_swarm_count;
        
        for(int j = 0; j < in_swarm_count; ++j){
            
            // * Primim numarul de segmente
            MPI_Recv(&segment_count, 1, MPI_UNSIGNED, TRACKER_RANK, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD, &status);
            int from_rank;
            MPI_Recv(&from_rank, 1, MPI_INT, TRACKER_RANK, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD, &status);

            // * Primim segmentele in sine(Hash-urile)
            for(int k = 0; k < segment_count; ++k){
                MPI_Recv(segments[k].hash, HASH_SIZE, MPI_CHAR, TRACKER_RANK, HASH_TAG, MPI_COMM_WORLD, &status);
            }
            
            // * Alocam spatiu pentru peersi, o singura data
            if(!peers_arr[i].peers_array){
                peers_arr[i].peers_array = (peer_info*)malloc(sizeof(peer_info) * in_swarm_count);
            }

            peers_arr[i].peers_array[j].file_id = atoi(&file_name[strlen(file_name) - 1]);
            peers_arr[i].peers_array[j].peer_rank = from_rank;
            peers_arr[i].peers_array[j].segment_count = segment_count;
            memcpy(peers_arr[i].peers_array[j].segments, segments, sizeof(file_segment) * segment_count);
        }

        // * Eliberam
        free(ranks_in_swarm);
    }

    #if defined(SHOW_RECV_SWARM_DEBUG) && (SHOW_RECV_SWARM_DEBUG > 0)
        // * Testam ce am primit
        printf("--------------\n");
        for(size_t i = 0; i < wanted_files_count; ++i){
            printf("Rank client %d\n", client->client_rank);
            printf("Rezumat pentru fișierul %s:\n", client->wanted_files[i].file_name);
            for(int j = 0; j < client->peers[i].peers_count; ++j){
                printf("Peer Rank: %d, Segment Count: %ld\n", 
                    client->peers[i].peers_array[j].peer_rank, 
                    client->peers[i].peers_array[j].segment_count);
                printf("%s\n%s\n", client->peers[i].peers_array[j].segments[0].hash,
                client->peers[i].peers_array[j].segments[client->peers[i].peers_array[j].segment_count - 1].hash);
            }
        }
        printf("--------------\n");
    #endif

}


void *download_thread_func(void *arg) {
    client_files* client = (client_files*)arg;
    size_t wanted_files_count = client->wanted_files_count;

    request_seeders_peers_list(client);

    char buff[BUFF_SIZE];
    int downloaded_segments = 0;
    MPI_Status status;
    int wanted_file_idx = 0;
    // * Seederii nu au nimic de descarcat
    bool should_download = true;
    srand(time(NULL));

    /*
        * Am pus si a doua conditie pentru cazul in care clientul
        * este seeder din start.
    */
    while(should_download){
        int peers_count = client->peers[wanted_file_idx].peers_count;
        
        // * doar ca sa fim siguri i guess(for some reason, la debugging uneori peers_count avea garbage value)
        if(peers_count <= 0){
            printf("No peers available for file index %d\n", wanted_file_idx);
            ++wanted_file_idx;
            continue;
        }

        int rand_peer = (peers_count > 1) ? rand() % peers_count : 0;
        peer_info* peer = &client->peers[wanted_file_idx].peers_array[rand_peer];

        char* f_name = client->wanted_files[wanted_file_idx].file_name;
        int wanted_file_id = atoi(&f_name[strlen(f_name) - 1]);

        if (!file_is_owned(client, wanted_file_id)) {
            add_file_to_owned(client, wanted_file_id);
        }

        file_data* c_data = find_file_data(client->owned_files, client->owned_files_count, wanted_file_id);
        // * Ceva teribil s-a intamplat..
        assert(c_data != nullptr);

        bool has_downloaded_segment = false;

        for(size_t k = c_data->segment_count; k < peer->segment_count; ++k){
            file_segment file_seg = peer->segments[k];

            if (!has_segment(c_data, file_seg)) {
                MPI_Send(file_seg.hash, HASH_SIZE - 1, MPI_CHAR, peer->peer_rank, REQUEST_TAG, MPI_COMM_WORLD);
                MPI_Recv(buff, BUFF_SIZE, MPI_CHAR, peer->peer_rank, ACK_TAG, MPI_COMM_WORLD, &status);

                // * A primit segmentul de la peer/seeder
                if(strcmp(buff, "OK") == 0){
                    // * Practic il marcam ca primit
                    add_segment_to_file_data(c_data, file_seg);
                    ++downloaded_segments;
                    has_downloaded_segment = true;
                    
                    // * Pentru a descarca de la cat mai multi random peers/seeders
                    break;
                }
            }
        }

        // * A descarcat unul dintre fisierele dorite, il salveaza
        if(!has_downloaded_segment){
            // * Daca are fisiere descarcate pe care nu le-atrimis
            if(downloaded_segments > 0){
                MPI_Send("DOWN_X", 8, MPI_CHAR, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);
                MPI_Send(&c_data->file_id, 1, MPI_INT, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);
                // * Trimit catre tracker segmentele noi :o
                for(size_t i = c_data->segment_count - 10; i < c_data->segment_count; ++i){
                    MPI_Send(c_data->segments[i].hash, HASH_SIZE, MPI_CHAR, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);
                }

                downloaded_segments = 0;
            }

            // * Trece la urmatorul fisier de descarcat, daca exista
            ++wanted_file_idx;

            file_data* downloaded_file = find_file_data(client->owned_files, client->owned_files_count, wanted_file_id);
            char out_file_name[18];
            sprintf(out_file_name, "client%d_file%d", client->client_rank, wanted_file_id);
            write_to_file(out_file_name, downloaded_file);

            if(wanted_file_idx >= wanted_files_count){
                break;
            }
        }

        if(downloaded_segments > 0 && downloaded_segments % 10 == 0){
            MPI_Send("DOWN_10", 8, MPI_CHAR, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);
            MPI_Send(&c_data->file_id, 1, MPI_INT, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);
            // * Trimit catre tracker segmentele noi :o
            for(size_t i = c_data->segment_count - 10; i < c_data->segment_count; ++i){
                MPI_Send(c_data->segments[i].hash, HASH_SIZE, MPI_CHAR, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);
            }

            downloaded_segments = 0;

            MPI_Send("GIVE_PEERS", 11, MPI_CHAR, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);
            MPI_Recv(buff, BUFF_SIZE, MPI_CHAR, TRACKER_RANK, ACK_TAG, MPI_COMM_WORLD, &status);
            if(strcmp(buff, "OK") == 0){
                printf("Requested peers, client %d :P\n", client->client_rank);
            }
        }
    }

    #if defined(SHOW_DOWNLOAD_DEBUG) && (SHOW_DOWNLOAD_DEBUG > 0)
        printf("Descarcare terminata. Segments: %d, Rank: %d\n", downloaded_segments, client->client_rank);
        printf("Gata %d\n", client->client_rank);
        if(client->client_rank == 3){
            printf("file %s\n", client->owned_files[0].file_name);
            for(size_t i = 0; i < client->owned_files[0].segment_count; ++i){
                printf("%s\n", client->owned_files[0].segments[i].hash);
            }
        }
    #endif

    MPI_Send("FINISHED_DOWN_ALL", 18, MPI_CHAR, TRACKER_RANK, INFORM_TAG, MPI_COMM_WORLD);

    return nullptr;
}

void *upload_thread_func(void *arg)
{
    char buff[BUFF_SIZE];
    MPI_Status status;

    while(true){
        
        MPI_Recv(buff, BUFF_SIZE, MPI_CHAR, MPI_ANY_SOURCE, REQUEST_TAG, MPI_COMM_WORLD, &status);
        if(strcmp(buff, "STOP_UPLOADING") == 0){
            break;
        }

        MPI_Send("OK", 2, MPI_CHAR, status.MPI_SOURCE, ACK_TAG, MPI_COMM_WORLD);
    }

    return NULL;
}

// * Send to all the clients, at start up
void send_peers_to_clients(tracker_ds* m_tracker){
    MPI_Status status;
    swarm* m_swarm = m_tracker->swarms;
    CLIENT_TYPE client_type;

    for(int i = 0; i < m_tracker->client_count; ++i){
    // * E seeder, nu doreste nimic, trecem peste
        if(m_tracker->data[i].client_type == SEEDER)
            continue;

        MPI_Recv(&client_type, 1, MPI_INT, MPI_ANY_SOURCE, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD, &status);
        int client_rank = status.MPI_SOURCE;
        m_tracker->data[client_rank - 1].client_type = client_type;

        // * Doreste fisiere
        size_t wanted_file_count;
        MPI_Recv(&wanted_file_count, 1, MPI_UNSIGNED, client_rank, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD, &status);

        int* files_id = (int*)malloc(wanted_file_count * sizeof(int));
        MPI_Recv(files_id, wanted_file_count, MPI_INT, client_rank, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD, &status);

        // * Ii trimitem lista de peers/seeders
        for(int i = 0; i < wanted_file_count; ++i){
            int wanted_swarm = files_id[i];

            int in_swarm_count = m_swarm[wanted_swarm - 1].clients_in_swarm_count;
            int* file_swarm = m_swarm[wanted_swarm - 1].clients_in_swarm;
            MPI_Send(&in_swarm_count, 1, MPI_INT, client_rank, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD);
            MPI_Send(file_swarm, in_swarm_count, MPI_INT, client_rank, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD);
            
            // * Trimitem segmentele asociate fiecarui client din swarm si numarul lor
            for(int j = 0; j < in_swarm_count; ++j){                    
                // * Trimitem segmentele asociate cu fisierul pe care il doreste clientul
                // * file_swarm[j] - clientul/rankul in care cautam, tracker->data[0] - client 1 and so on...
                int client_num_of_files = m_tracker->data[file_swarm[j] - 1].files_count;
                file_data* client = m_tracker->data[file_swarm[j] - 1].files;

                // * Iteram prin fisierele clientului si il alegem pe cel pe care il doreste clientul
                for(int k = 0; k < client_num_of_files; ++k){
                    if(wanted_swarm == client[k].file_id){
                        MPI_Send(&client[k].segment_count, 1, MPI_UNSIGNED, client_rank, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD);
                        
                        // * Trimitem rankul
                        MPI_Send(&file_swarm[j], 1, MPI_INT, client_rank, PEERS_SEEDERS_TRANSFER_TAG, MPI_COMM_WORLD);                            
                        
                        // * Trimitem segmentele(hash-urile)
                        for(int l = 0; l < client[k].segment_count; ++l){
                            MPI_Send(client[k].segments[l].hash, HASH_SIZE, MPI_CHAR, client_rank, HASH_TAG, MPI_COMM_WORLD);
                        }
                        
                        break;
                    }
                }
            }
        }

        free(files_id);
    }
}

void update_tracker_swarm(tracker_ds* m_tracker, int rank, char* buff){
    int file_id = 0;
    MPI_Recv(&file_id, BUFF_SIZE, MPI_CHAR, rank, INFORM_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    
    /*
        * Aceeasi logica ca la retriev-ul fisierului respectiv ca la clienti.
        * Daca nu exista, il creeam gol si urmeaza sa il populam.
    */
    if(!tracker_client_has_file(m_tracker, file_id, rank - 1))
        tracker_add_file_to_owned(m_tracker, file_id, rank - 1);

    file_data* c_data = find_file_data(m_tracker->data[rank - 1].files, 
                                    m_tracker->data[rank - 1].files_count,
                                    file_id);

    for(size_t i = 0; i < 10; ++i){
        MPI_Recv(buff, BUFF_SIZE, MPI_CHAR, rank, INFORM_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        strcpy(c_data->segments[c_data->segment_count].hash, buff);
        ++c_data->segment_count;
    }

    // * Aceasta functie le si updateaza daca deja exista
    create_file_swarms(m_tracker, m_tracker->client_count + 1);
}

void tracker(tracker_ds* m_tracker){
    // * Trimitem fiecarui client swarm-ul fisierelor respective
    send_peers_to_clients(m_tracker);
    
    int downloading_clients = 0;
    for(int i = 0; i < m_tracker->client_count; ++i){
        if(m_tracker->data[i].client_type == SEEDER)
            continue;

        ++downloading_clients;
    }

    // * Show tracker info before 
    #if defined(SHOW_TRACKER_DEBUG) && (SHOW_TRACKER_DEBUG > 0)
        show_tracker(m_tracker, m_tracker->client_count + 1);        
    #endif

    MPI_Status status;
    char buff[BUFF_SIZE];
    int finished_clients = 0;
    bool should_track = true;

    while(should_track){
        MPI_Recv(buff, BUFF_SIZE, MPI_CHAR, MPI_ANY_SOURCE, INFORM_TAG, MPI_COMM_WORLD, &status);

        if(strcmp(buff, "FINISHED_DOWN_ALL") == 0){
            // * Il marcam ca SEEDER(in cazul in care este LEECHER, acesta va ramane LEECHER pe veci)
            CLIENT_TYPE* cl_type = &m_tracker->data[status.MPI_SOURCE - 1].client_type; 
            if(*cl_type == PEER)
                *cl_type = SEEDER;

            ++finished_clients;
        // * Informam tracker-ul cu ce segmente noi detine clientul
        }else if(strcmp(buff, "DOWN_10") == 0){
            int rank = status.MPI_SOURCE;
            update_tracker_swarm(m_tracker, rank, buff);

            MPI_Send("OK", 2, MPI_CHAR, rank, ACK_TAG, MPI_COMM_WORLD);
        }else if(strcmp(buff, "DOWN_X") == 0){
            int rank = status.MPI_SOURCE;
            update_tracker_swarm(m_tracker, rank, buff);
        }else if(strcmp(buff, "GIVE_PEERS") == 0){
            printf("Updated peers :P\n");
        }

        if(finished_clients == downloading_clients){
            printf("END OF TRACKING\n");
            should_track = false;
        }
    }

    // * Le spunem clientilor din retea ca se pot oprii din upload
    for(int rank = 1; rank <= m_tracker->client_count; ++rank){
        if(m_tracker->data[rank - 1].client_type != LEECHER)
            MPI_Send("STOP_UPLOADING", 15, MPI_CHAR, rank, REQUEST_TAG, MPI_COMM_WORLD);
    }

    // * Show tracker info at the end
    #if defined(SHOW_TRACKER_DEBUG) && (SHOW_TRACKER_DEBUG > 0)
        show_tracker(m_tracker, m_tracker->client_count + 1);        
    #endif
}

void peer(int numtasks, int rank, client_files* client) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;
    
    // * Seederii nu au nimic de descarcat
    if(client->client_type != SEEDER){
        r = pthread_create(&download_thread, NULL, download_thread_func, (void*)client);
        if (r) {
            printf("Eroare la crearea thread-ului de download\n");
            exit(-1);
        }
    }

    // * Leecherii nu au nimic de uploadat
    if(client->client_type != LEECHER){
        r = pthread_create(&upload_thread, NULL, upload_thread_func, (void*)&numtasks);
        if (r) {
            printf("Eroare la crearea thread-ului de upload\n");
            exit(-1);
        }
    }

    if(client->client_type != SEEDER){
        r = pthread_join(download_thread, &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului de download\n");
            exit(-1);
        }
    }

    if(client->client_type != LEECHER){
        r = pthread_join(upload_thread, &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului de download\n");
            exit(-1);
        }
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    client_files client_file;
    tracker_ds m_tracker;
    // * Puteam folosi calloc din start
    memset(&client_file, 0, sizeof(client_files));
    memset(&m_tracker, 0, sizeof(tracker_ds)); 

    if (rank == TRACKER_RANK) {
        receive_data_from_clients(&m_tracker, numtasks);
        tracker(&m_tracker);
        free_tracker(&m_tracker);
    } else {
        read_from_file(&client_file, rank);
        send_data_to_tracker(&client_file);

        // * Primim confirmare de la tracker
        char ack[3];
        MPI_Status mpi_status;
        MPI_Recv(ack, 2, MPI_CHAR, TRACKER_RANK, ACK_TAG, MPI_COMM_WORLD, &mpi_status);

        peer(numtasks, rank, &client_file);
        free_client_files(&client_file);
    }

    MPI_Finalize();
}