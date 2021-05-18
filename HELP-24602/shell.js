load("/Users/kevin.albertson/code/mongo/jstests/libs/parallelTester.js")

nthreads = 100
function read() {
    threads = []
    for (var t=0; t<nthreads; t++) {
        thread = new Thread(function(t) {
            while (true)
                db.c.findOne({_id: 0})
        }, t)
        threads.push(thread)
        thread.start()
    }
    for (var t = 0; t < nthreads; t++)
        threads[t].join()
}

print ("running shell workload with " + nthreads + " threads")
read()