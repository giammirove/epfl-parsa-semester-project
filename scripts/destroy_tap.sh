if [ $# -eq 0 ]
  then
    echo "No arguments supplied"
    exit
fi

i=$1

sudo ip link del br$i
sudo ip link del tap$i
sudo ip link del tap$i-ns


