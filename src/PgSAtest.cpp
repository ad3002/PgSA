#include "index/cache/persistence/CountQueriesCachePersistence.h"
#include "suffixarray/persistence/SuffixArrayPersistence.h"
#include "test/PgSATests.h"

#include "helper.h"
#include <stdlib.h>    /* for exit */
#include <unistd.h>

#include <string>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


PgSAIndexStandard* prepareIndex(string idxFile, string cacheFile) {

    return PgSAIndexFactory::getPgSAIndexStandard(idxFile, cacheFile, false);

};

int main(int argc, char *argv[])
{

    int opt; // current option
    string cacheFile;
    string kParam;
    int size = 0;

    string read_file_name;
    int sockfd, newsockfd, portno;
    portno = 3015;

    while ((opt = getopt(argc, argv, "fp:p:f:?")) != -1) {
        switch (opt) {
        case 'p':
            portno = atoi(optarg);
            break;
        case 'f':
            read_file_name = optarg;
            break;
        case '?':
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-p port] [-f read file name] indexfile\n\n",
                    argv[0]);
            fprintf(stderr, "\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind != (argc - 1)) {
        fprintf(stderr, "%s: Expected only index name after options\n", argv[0]);
        fprintf(stderr, "try '%s -?' for more information\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Loading PgSA...\n");

    string idxFile(argv[optind++]);

    PgSAIndexStandard* pgsaIndex;
    pgsaIndex = prepareIndex(idxFile, cacheFile);
    vector<StandardOccurrence> q3res;
    std::ifstream read_file;

    
    fprintf(stderr, "Ready to query\n");

 
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
          sizeof(serv_addr)) < 0) 
          error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, 
             (struct sockaddr *) &cli_addr, 
             &clilen);
    if (newsockfd < 0) 
      error("ERROR on accept");


    uint32_t converted_number;


     while (1) {
         bzero(buffer,256);
         
         n = read(newsockfd, buffer, 255);
         if (n < 0) error("ERROR reading from socket");
         
         if (n < 13) continue;

         printf("Get %d bytes \n", n);
         printf("Query PgSA with: %s\n", buffer);
         pgsaIndex->reportOccurrences(buffer, q3res); 
         size = q3res.size();
         printf("Done. Result %d items\n",size);

         converted_number = htonl(size);
         n = write(newsockfd, &converted_number, sizeof(uint32_t));
         if (n < 0) error("ERROR writing to socket");

         for (int j=0; j<size; j++) {
             std::cout << "\t" << q3res[j].first << "," << q3res[j].second << std::endl;
             converted_number = htonl(q3res[j].first);
             n = write(newsockfd, &converted_number, sizeof(uint32_t));
             if (n < 0) error("ERROR writing to socket");
             converted_number = htonl(q3res[j].second);
             n = write(newsockfd, &converted_number, sizeof(uint32_t));
             if (n < 0) error("ERROR writing to socket");
         }
         printf("Sending done");
     }

     close(newsockfd);
     close(sockfd);

    
    // read_file.close();
    delete(pgsaIndex);

    exit(EXIT_SUCCESS);
    



    // int opt = 0; // current option
    // int repeat = 11;
    // int testKmersNumber = 100000;
    // vector<unsigned short> kLengths = {11, 16, 22};
    // bool sFlag = false; // scramble reads (for uncorrecly concatenated pair-ended data)
    // bool fFlag = false; // filter TTTTTT.....TTTT reads (for compatibility with CGk tests)
    // bool pFlag = false; // query by position
    // string cacheFile = "";
    // string kParam = "";
    // size_t pos = 0;
    // size_t found = 0;

    // while ((opt = getopt(argc, argv, "sfpr:n:c:k:?")) != -1) {
    //     switch (opt) {
    //     case 'r':
    //         repeat = atoi(optarg);
    //         break;
    //     case 'n':
    //         testKmersNumber = atoi(optarg);
    //         break;
    //     case 'c':
    //         cacheFile = optarg;
    //         break;
    //     case 'k':
    //         kLengths.clear();
    //         kParam = optarg;
    //         pos = 0;
    //         found = 0;
    //         while((found = kParam.find_first_of(",;|", pos)) != string::npos) {
    //             kLengths.push_back(atoi(kParam.substr(pos, found - pos).c_str()));
    //             pos = found + 1;
    //         }
    //         kLengths.push_back(atoi(kParam.substr(pos).c_str()));
    //         break;
    //     case 's':
    //         sFlag = true;
    //         break;
    //     case 'f':
    //         fFlag = true;
    //         break;
    //     case 'p':
    //         pFlag = true;
    //         break;
    //     case '?':
    //     default: /* '?' */
    //         fprintf(stderr, "Usage: %s [-k length] [-r no of repeats] [-n no of testkmers] [-c cachefile] [-p] [-s] [-f] indexfile\n\n",
    //                 argv[0]);
    //         fprintf(stderr, "-p query by position\n-s scramble reads (for uncorrecly concatenated pair-ended data)\n-f -filter TTTTTT.....TTTT reads (for compatibility with CGk tests)\n\n");
    //         exit(EXIT_FAILURE);
    //     }
    // }

    // if (optind != (argc - 1)) {
    //     fprintf(stderr, "%s: Expected only index name after options\n", argv[0]);
    //     fprintf(stderr, "try '%s -?' for more information\n", argv[0]);
    //     exit(EXIT_FAILURE);
    // }


    // string idxFile(argv[optind++]);

    // PgSAIndexStandard* idx = prepareIndex(idxFile, cacheFile);

    // cout << "*****************************************************************************\n";
    // cout << idx->getDescription();

    // for(unsigned int i = 0; i < kLengths.size(); i++)
    //     PgSATest::runTest(idx, repeat, testKmersNumber, kLengths[i], sFlag, fFlag, pFlag);

    // delete(idx);

    // exit(EXIT_SUCCESS);
}
