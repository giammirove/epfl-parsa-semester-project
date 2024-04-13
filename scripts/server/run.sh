DIR="../../images/buildroot/build_aarch64/images"

quanta=${1:-10000}
echo $quanta

# exec foot sh -c "sleep 100 | sudo $DIR/start.sh 3 64 4 $DIR/config.txt $quanta" &
# exec foot sh -c "sleep 100 | sudo $DIR/start.sh 2 64 4 $DIR/config.txt $quanta" &
exec foot sh -c "sleep 100 | sudo $DIR/start.sh 1 64 4 $DIR/config.txt $quanta" &
exec foot sh -c "sleep 100 | sudo $DIR/start.sh 0 64 2 $DIR/config.txt $quanta" &

gcc main.c -Werror -Wall -o main && ./main "$DIR/config.txt" $quanta


