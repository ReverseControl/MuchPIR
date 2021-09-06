FROM ubuntu:20.04 

#Install utilities we will need to update dev packagges
RUN apt-get update && apt install -y wget sudo gnupg lsb-release && apt-get clean all

#Update repo to get latest stable postgres version
RUN wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
RUN echo "deb http://apt.postgresql.org/pub/repos/apt/ `lsb_release -cs`-pgdg main" | sudo tee  /etc/apt/sources.list.d/pgdg.list

#Install development packages we will need
RUN apt update                                &&  \ 
    DEBIAN_FRONTEND=noninteractive apt install -y \
        gcc-10                                    \
        g++-10                                    \
        git                                       \ 
        make                                      \
        cmake                                     \
        postgresql-13                             \
        postgresql-client-13                      \
        postgresql-server-dev-13                  \
        libpq-dev                             &&  \
     apt clean all
        
#Set gcc-10 and g++-10 as default compilers
RUN sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 10 && \
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 10

#Install libpqxx
WORKDIR /git
RUN git clone https://github.com/jtv/libpqxx.git && \
    cd libpqxx                                   && \
    git checkout 7.2.0                           && \
    ./configure --disable-documentation          && \
    make                                         && \
    make install

#Install Microsoft SEAL
WORKDIR /git
RUN git clone https://github.com/microsoft/SEAL.git && \
    cd SEAL                                         && \
    git checkout v3.5.8                             && \
    cmake -DBUILD_SHARED_LIBS=ON .                  && \
    make                                            && \
    make install       

#Install MuchPIR
WORKDIR /git
RUn git clone https://github.com/ReverseControl/MuchPIR.git && \
    cd MuchPIR                                              && \
    make                                                    && \
    make install

#For editing if need be
RUN apt update && apt install -y vim

#Run the remaining command as postgres user
USER postgres

#Setup postgres
RUN /etc/init.d/postgresql start                                          && \
    psql --command "CREATE USER docker WITH SUPERUSER PASSWORD 'docker';" && \
    createdb -O docker   docker                                           && \
    createdb -O postgres testdb

# Adjust PostgreSQL configuration so that remote connections to the database are possible.
RUN echo "host all  all    0.0.0.0/0  md5" >> /etc/postgresql/13/main/pg_hba.conf
RUN echo "listen_addresses='*'"            >> /etc/postgresql/13/main/postgresql.conf

# Expose the PostgreSQL port
EXPOSE 5432

# Add VOLUMEs to allow backup of config, logs and databases
#VOLUME  ["/etc/postgresql", "/var/log/postgresql", "/var/lib/postgresql"]


#Add data to test C/C++ Postgres Aggregate Extension inside the container
WORKDIR /data/
COPY load_data.sql /data/
COPY ./sample_imdb.tsv /data/

#Tell postgres where to find libseal.so
ENV LD_LIBRARY_PATH="/usr/local/lib/"

#Meta tags
LABEL author="ReverseControl"
LABEL version="1.0.0"
LABEL date="04/09/2021"
LABEL description="Image to run MuchPIR on Postgres 13 in a container."
LABEL version_seal="3.5.8"
LABEL version_liboqxx="7.2.0"
LABEL gcc_compiler="gcc-10"
LABEL g++_compiler="g++-10"

# Set the default command to run when starting the container
CMD ["/usr/lib/postgresql/13/bin/postgres", "-D", "/var/lib/postgresql/13/main", "-c", "config_file=/etc/postgresql/13/main/postgresql.conf"]
