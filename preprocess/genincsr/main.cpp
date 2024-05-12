#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define uintV_t uint32_t
#define uintE_t uint64_t


size_t fsize(const std::string& fname){
    struct stat st;
    if (0 == stat(fname.c_str(), &st)) {
        return st.st_size;
    }
    perror("stat issue");
    return -1L;
}

/* -------------------------------------------------------------- */
// ALLOC in SSD -- use mmap
void* ssd_alloc(const char* filepath, size_t size){
    int fd = open(filepath, O_RDWR|O_CREAT, 00777);
    if (fd == -1){
      std::cout << "Could not open file for :" << filepath << " error: " << strerror(errno) << std::endl;
      exit(1);
    }
    if (ftruncate(fd, size) == -1) {
      std::cout << "Could not ftruncate file for :" << filepath << " error: " << strerror(errno) << std::endl;
      close(fd);
      exit(1);
    }
    char* addr = (char*)mmap(NULL, size, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
    close(fd);
    if (addr == (char*)MAP_FAILED) {
        std::cout << "Could not mmap for :" << filepath << " error: " << strerror(errno) << std::endl;
        std::cout << "size = " << size << std::endl;
        exit(1);
    }
    return addr;
}

void* mmap_read(const char* filepath, size_t size) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
      std::cout << "Could not open file for :" << filepath << " error: " << strerror(errno) << std::endl;
      exit(1);
    }
    char* addr = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (addr == (char*)MAP_FAILED) {
        std::cout << "Could not mmap for :" << filepath << " error: " << strerror(errno) << std::endl;
        exit(1);
    }
    return addr;
}


int main(int argc, char** argv) {
    // [exe] [dataset path] [dataset name]
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " [dataset path] [dataset name]" << std::endl;
        return 1;
    }
    std::string dataset_path = argv[1];
    std::string dataset_name = argv[2];

    std::cout << "dataset path: " << dataset_path << std::endl;
    std::cout << "dataset name: " << dataset_name << std::endl;

    std::string idx_path = dataset_path + "/" + dataset_name + ".idx";
    std::string adj_path = dataset_path + "/" + dataset_name + ".adj";

    size_t idx_size = fsize(idx_path), adj_size = fsize(adj_path);
    if (idx_size == -1 || adj_size == -1) {
        std::cout << "Error: could not get file size" << std::endl;
        return 1;
    }

    uintV_t nverts = idx_size / sizeof(uintE_t)-1;
    uintE_t nedges = adj_size / sizeof(uintV_t);

    uintE_t* idx = (uintE_t*)mmap_read(idx_path.c_str(), idx_size);
    uintV_t* adj = (uintV_t*)mmap_read(adj_path.c_str(), adj_size);

    // read idx and adj using mmap

    std::string ridx_path = dataset_path + "/" + dataset_name + ".ridx";
    std::string radj_path = dataset_path + "/" + dataset_name + ".radj";

    size_t ridx_size = (nverts) * sizeof(uintE_t);
    size_t radj_size = (nedges) * sizeof(uintV_t);

    uintE_t* ridx = (uintE_t*)ssd_alloc(ridx_path.c_str(), ridx_size);
    uintV_t* radj = (uintV_t*)ssd_alloc(radj_path.c_str(), radj_size);

    memset(ridx, 0, ridx_size);
    memset(radj, 0, radj_size);

    // calculate reverse index
    uintE_t* rdegree = new uintE_t[nverts];
    for (uintV_t u = 0; u < nverts; u++) {
        uintE_t start = idx[u];
        uintE_t end = idx[u+1];
        for (uintE_t i = start; i < end; i++) {
            uintV_t v = adj[i];
            rdegree[v]++;
        }
    }
    for (uintV_t u = 1; u < nverts; u++) {
        ridx[u] = ridx[u-1] + rdegree[u-1];
    }
    delete[] rdegree;

    // calculate reverse adj
    uintE_t* ridx_ptr = new uintE_t[nverts];
    memcpy(ridx_ptr, ridx, nverts * sizeof(uintE_t));
    for (uintV_t u = 0; u < nverts; u++) {
        uintE_t start = idx[u];
        uintE_t end = idx[u+1];
        for (uintE_t i = start; i < end; i++) {
            uintV_t v = adj[i];
            radj[ridx_ptr[v]++] = u;
        }
    }

    delete[] ridx_ptr;

    munmap(idx, idx_size);
    munmap(adj, adj_size);

    // save ridx and radj
    msync(ridx, ridx_size, MS_SYNC);
    msync(radj, radj_size, MS_SYNC);

    munmap(ridx, ridx_size);
    munmap(radj, radj_size);

    return 0;
}