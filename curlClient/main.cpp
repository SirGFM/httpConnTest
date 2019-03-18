#include <curl/curl.h>
#include <exception>
#include <cstdio>
#include <string>

/* ===== Debug stuff... ================================================ */
extern "C" {

static
void dump(const char *text,
        FILE *stream, unsigned char *ptr, size_t size)
{
    size_t i;
    size_t c;
    unsigned int width=0x10;

    fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
            text, (long)size, (long)size);

    for(i=0; i<size; i+= width) {
        fprintf(stream, "%4.4lx: ", (long)i);

        /* show hex to the left */
        for(c = 0; c < width; c++) {
            if(i+c < size)
                fprintf(stream, "%02x ", ptr[i+c]);
            else
                fputs("   ", stream);
        }

        /* show data on the right */
        for(c = 0; (c < width) && (i+c < size); c++) {
            char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x80) ? ptr[i+c] : '.';
            fputc(x, stream);
        }

        fputc('\n', stream); /* newline */
    }
}

static
int my_trace(CURL *handle, curl_infotype type,
        char *data, size_t size,
        void *userp)
{
    const char *text;
    (void)handle; /* prevent compiler warning */
    (void)userp;

    switch (type) {
        case CURLINFO_TEXT:
            fprintf(stderr, "== Info: %s", data);
        default: /* in case a new one is introduced to shock us */
            return 0;

        case CURLINFO_HEADER_OUT:
            text = "=> Send header";
            break;
        case CURLINFO_DATA_OUT:
            text = "=> Send data";
            break;
        case CURLINFO_SSL_DATA_OUT:
            text = "=> Send SSL data";
            break;
        case CURLINFO_HEADER_IN:
            text = "<= Recv header";
            break;
        case CURLINFO_DATA_IN:
            text = "<= Recv data";
            break;
        case CURLINFO_SSL_DATA_IN:
            text = "<= Recv SSL data";
            break;
    }

    dump(text, stderr, (unsigned char *)data, size);
    return 0;
}

} /* extern "C" */


/* ===== Init & Clean cURL ============================================= */

class GlobalCurlInit {
public:
    GlobalCurlInit() {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ~GlobalCurlInit() {
        curl_global_cleanup();
    }
};
static GlobalCurlInit _initCurl;

class CurlException : public std::exception {
private:
    std::string err;
public:
    CurlException(std::string err) : err(err) {}
    ~CurlException() {}
    const char *what() const noexcept {
        return this->err.c_str();
    }
};


/* ===== Base cURL class =============================================== */

class MemStream {
private:
    bool output;
public:
    FILE *fp;

    MemStream(size_t size) {
        this->fp = fmemopen(nullptr, size+1, "w+");
        if (!fp)
            throw CurlException("Failed to open 'write' buf");
        this->output = true;
    }

    MemStream(unsigned char *data, size_t size) {
        this->fp = fmemopen(data, size, "r");
        if (!fp)
            throw CurlException("Failed to open 'read' buf");
        this->output = false;
    }

    std::string read() {
        if (!this->output)
            throw CurlException("Buf not set for reading");
        long size = ftell(this->fp);
        fflush(this->fp);
        unsigned char tmp[size];
        fseek(this->fp, 0, SEEK_SET);
        fread(tmp, 1, size, this->fp);
        return std::string((char*)(tmp), size);
    }

    ~MemStream() {
        fclose(this->fp);
    }
};

class CurlList {
private:
    struct curl_slist *slist;
public:
    CurlList() {
        this->slist = nullptr;
    }

    ~CurlList() {
        if (this->slist)
            curl_slist_free_all(this->slist);
    }

    void add(std::string header) {
        this->slist = curl_slist_append(this->slist, header.c_str());
        if (!this->slist)
            throw CurlException("Failed to append to list");
    }

    void set(CURL *curl) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, this->slist);
    }
};

class CurlClient {
private:
    CURL *curl;
    std::string url;
    bool verbose;
    bool debug;

    void setVerbose() {
        if (this->verbose)
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        if (this->debug)
            curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
    }

    void send() {
        CURLcode ret;

        this->setVerbose();
        ret = curl_easy_perform(this->curl);
        if(ret != CURLE_OK)
            throw CurlException(std::string(curl_easy_strerror(ret)));
    }
public:
    CurlClient(bool verbose, bool debug) : verbose(verbose), debug(debug) {
        this->curl = curl_easy_init();
        if (!this->curl)
            throw CurlException("Failed to retrieve a CURL easy handle");
    }

    ~CurlClient() {
        curl_easy_cleanup(this->curl);
    }

    void connect(std::string _url) {
        if (this->url.length())
            throw CurlException("Already connected!");

        curl_easy_setopt(this->curl, CURLOPT_URL, _url.c_str());
        curl_easy_setopt(this->curl, CURLOPT_CONNECT_ONLY, 1L);
        this->send();

        this->url = _url;
    }

    std::string post(std::string data) {
        if (!this->url.length())
            throw CurlException("Not connected yet!");
        curl_easy_reset(this->curl);

        curl_easy_setopt(this->curl, CURLOPT_URL, this->url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        CurlList list;
        list.add("Content-Length: " + std::to_string(data.length()));
        list.set(this->curl);

        MemStream input((unsigned char*)(data.c_str()), data.length());
        curl_easy_setopt(this->curl, CURLOPT_READDATA, input.fp);

        MemStream output(data.length());
        curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, output.fp);
        this->send();

        return output.read();
    }
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s url msg [flag:debug]\n", argv[0]);
        return 1;
    }

    bool debug = (argc >= 4);
    CurlClient client(debug, debug);
    client.connect(argv[1]);
    printf("Sending message: \"%s\"...\n", argv[2]);
    std::string echo = client.post(argv[2]);
    printf("Got reply: \"%s\"!\n", echo.c_str());
    printf("Sending message (again): \"%s\"...\n", argv[2]);
    echo = client.post(argv[2]);
    printf("Got reply: \"%s\"!\n", echo.c_str());

    return 0;
}
