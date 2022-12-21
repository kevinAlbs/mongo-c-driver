# Repeatedly restart a server.
mlaunch start --dir .menv
read -p "Press enter to begin restart cycle"
while true; do
    time mlaunch stop --dir .menv
    time mlaunch start --dir .menv
    sleep 2
done
