package server

import (
    "io"
    "log"
    "net/http"
)

type defaultServer struct{}

func GetDefault() HttpServer {
    return &defaultServer{}
}

func (*defaultServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    log.Printf("Got new request from '%s'", r.RemoteAddr)

    if r.Method != "POST" {
        // Reply with invalid method
        log.Printf("Got invalid method '%s'", r.Method)
        w.Header().Add("Allow", "POST")
        w.WriteHeader(http.StatusMethodNotAllowed)
        return
    }

    var echo []byte
    for {
        var buf [1024]byte

        n, err := r.Body.Read(buf[:])
        echo = append(echo, buf[:n]...)

        if err == io.EOF {
            break
        } else if err != nil {
            log.Printf("Failed to read request: %+v", err)
            w.WriteHeader(http.StatusInternalServerError)
            return
        }
    }

    log.Printf("Message: '%s'", string(echo))

    // Copy headers into the response
    for k, varr := range r.Header {
        switch k {
        case "Content-Type":
            fallthrough
        case "Content-Length":
            for _, v := range varr {
                w.Header().Add(k, v)
            }
        }
    }
    w.WriteHeader(http.StatusOK)
    w.Write(echo)
}

func (self *defaultServer) Run(addr string) {
    s := http.Server{
        Addr:    addr,
        Handler: self,
        // ReadHeaderTimeout
        // IdleTimeout
    }
    log.Fatal(s.ListenAndServe())
}
