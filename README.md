this ddos c + and python root

you need to download packet

sudo apt update 

sudo apt-get install libpcap-dev git gcc

sudo apt-get install -y \
    build-essential \
    linux-headers-$(uname -r) \
    libc6-dev \
    libpthread-stubs0-dev

    gcc -O3 -o netdub netdub.c -lpthread -lm -lpcap

    sudo apt-get update && sudo apt-get install -y libpcap-dev git gcc build-essential linux-headers-$(uname -r) && echo "Installation complete!"


gcc -v -o netdub netdub.c -lpthread -lm
