# 🔄 BitTorrent Protocol Simulation using MPI

## 📝 Overview

This project simulates the BitTorrent peer-to-peer (P2P) file sharing protocol utilizing the Message Passing Interface (MPI). It models the decentralized distribution and downloading of files across a network, showcasing the efficiency of BitTorrent's segmented file transfer and peer collaboration.

## 🌟 Features

- **MPI-Based Simulation** 🌐: Uses MPI for simulating network communication, emulating the complex interactions of the BitTorrent ecosystem.
- **Peer Roles** 👥: Simulates seeders, peers, and leechers, each playing unique roles in the file-sharing process.
- **Segmented File Transfer** 📦: Demonstrates how files are divided into segments, facilitating efficient and scalable downloads.
- **Dynamic Peer Interaction** 💬: Implements logic for peers to dynamically find each other and exchange file segments.
- **Tracker Functionality** 🛰️: Features a simulated tracker to coordinate the sharing process, helping peers locate file segments.

## 🛠 Implementation Details

The project is structured into two main components: the tracker and the clients (peers), with the tracker orchestrating the network and clients participating in file sharing.

### Tracker and Clients Architecture

**Tracker**: Acts as the central coordinator within the network. It doesn't participate in file sharing but maintains a directory of which clients hold which segments of each file.

**Clients (Peers)**: Engage in the actual file sharing. Each client can act in one of three roles, depending on the files they possess and their actions in the network:

```c
typedef enum CLIENT_TYPE {
    SEEDER = 0, // Clients that have a complete copy of the file
    PEER,       // Clients that have parts of the file and are in the process of downloading or uploading
    LEECHER     // Clients that do not have the file but wish to download it
} CLIENT_TYPE;
```

### File Segmentation

Files are divided into segments to facilitate efficient distribution across the network. Each segment is uniquely identified by a hash, ensuring integrity and ease of tracking:

```cpp
typedef struct file_segment {
    char hash[HASH_SIZE + 1]; // HASH_SIZE is typically defined as 32
} file_segment;
```

### Client Initialization

Clients start by reading input files specifying their held and desired files, setting the stage for the simulation.

**Example:**
```c
// Reading client's files from input
void read_from_file(client_files* client, int rank) {
    // Opens the client input file and initializes client data
    ...
}
```

### File Sharing Process

Clients communicate with the tracker to discover peers and request or provide file segments.

Downloading Segments Example:

```cpp
// Download thread function
void *download_thread_func(void *arg) {
    client_files* client = (client_files*)arg;
    ...
    request_seeders_peers_list(client);
    ...
}
``` 

### Concurrency

The simulation leverages pthreads to manage concurrent downloads and uploads within each client, mimicking real-world peer-to-peer interactions.

Upload Thread Example:

```cpp
// Upload thread function
void *upload_thread_func(void *arg) {
    ...
    while(true){
        MPI_Recv(buff, BUFF_SIZE, MPI_CHAR, MPI_ANY_SOURCE, REQUEST_TAG, MPI_COMM_WORLD, &status);
        MPI_Send("OK", 2, MPI_CHAR, status.MPI_SOURCE, ACK_TAG, MPI_COMM_WORLD);
    }
    ...
}
```

## 📚 Building and Running

To run the simulation, compile the source code and execute it with MPI, specifying the number of processes (one for the tracker and the rest as clients).

Compilation:

```cpp
make build
// OR
mpicc -o <exec_name> -Wall main.c -pthread
```

Execution:

```cpp
make run
// OR
mpirun --oversubscribe -np <number_of_processes> <exec_name>
```

Check for memory leaks:

```cpp
make chleak
```

## Testing

Local Testing Script:

```cpp
cd checker && ./checker.sh
```

This script helps verify the simulation's correctness and performance on your local machine.

