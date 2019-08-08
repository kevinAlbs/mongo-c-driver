set -o xtrace
mongo --port 27017 --eval "db.adminCommand({'configureFailPoint': 'failCommand', 'mode': 'off'})"
mongo --port 27018 --eval "db.adminCommand({'configureFailPoint': 'failCommand', 'mode': 'off'})"
