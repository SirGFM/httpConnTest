package server

type HttpServer interface {
    Run(addr string)
}
