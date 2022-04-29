idf_component_register(SRCS
                        "rtp_io_main.c"
                    
                        "utils.c"
                        "ad_da.c"
                        "rtp_rx.c"
                        "jbuf.c"
                        "g711.c"
                        "g722/g722_decode.c"
                        "modbus_tcp/modbus_params.c"
                        "modbus_tcp/tcp_slave.c"
                        "di_do.c"
                        INCLUDE_DIRS "include")