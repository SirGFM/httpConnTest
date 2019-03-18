package main

import (
    "flag"
    "errors"
    "github.com/SirGFM/httpConnTest/server"
    "log"
    "strconv"
)

// ===== List all available servers =====================================
var serverMap = map[string](func()server.HttpServer) {
    "default": server.GetDefault,
}
func serverList() (s string) {
    for name := range serverMap {
        s += "\""+name+"\", "
    }
    return s[:len(s)-2]
}

// ===== flag.Var to set the server =====================================
type HttpMode struct {
    server server.HttpServer
    name string
}

func (h *HttpMode) String() string {
    return h.name
}

func (h *HttpMode) Set(s string) error {
    fn, ok := serverMap[s]
    if !ok {
        modes := serverList()
        return errors.New("Unknown mode '"+s+"'. Expected on of "+modes)
    }
    h.server = fn()
    h.name = s
    return nil
}

// ===== Entry point... =================================================
func main() {
    var fn HttpMode
    var port int

    flag.Var(&fn, "mode", "Server mode: "+serverList())
    flag.IntVar(&port, "port", 8080, "The server's listening port")
    flag.Parse()

    if fn.server == nil {
        panic("No mode set! Select one from "+serverList())
    }

    log.Print("Starting server...")
    fn.server.Run(":"+strconv.Itoa(port))
}
