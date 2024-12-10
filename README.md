# Προγραμματισμός Συστήματος - Εργασία 2
# Παπαδόπουλος Χρήστος
# ΑΜ: 1115202000165

# Παραδοτέα - Εκτέλεση
    •Όλα τα ζητούμενα χωρίς κάποια παράλειψη.
    •Τα προγράμματα εκτελούνται όπως περιγράφονται στην εκφώνηση.
    •Για compile εκτελέστε make. Για καθάρισμα make clean.
    •Το πρόγραμμα progDelay.c των tests απαιτεί ξεχωριστό compile.

# Δομές της εφαρμογής
    •Στο αρχείο jobExecutorServer.h έχει οριστεί η δομή Job η οποία περιέχει: το αναγνωριστικό κάθε δουλειάς, την δουλειά όπως δόθηκε από τον χρήστη και το file descriptor του socket, δύο βοηθητικές συναρτήσεις για τα format <jobId, job> και <jobId, job, socketFD> και μεταβλητές, mutexes και condition variables που χρησιμοποιούνται από τον server κατά την εκτέλεση.
    •Πιο συγκεκριμένα, στο header file υπάρχουν: 
        •Ο buffer, ορισμένος ως Job**(κατά την εκτέλεση γίνεται malloced σε Job*[bufferSize]).
        •Μεταβλητή για το jobID και το concurrency
        •Το socketFd(αρχικό socket που δέχεται connections), counters για δουλειές που τρέχουν/περιμένουν
        •Το mutex BufferSem με τα δύο condition variables bufferSpotCond, bufferNewJobCond όπου επιτρέπουν κάθε φορά σε ένα μόνο thread την είσοδο στο buffer
        •Το mutex concurrencySem με το condition variable concurrencyCond όπου επιτρέπει μόνο σε ένα thread τη φορά την αλλαγή του concurrency ή του runningJobs.    
    •Στον commander βρίσκεται μόνο η main.

# Επεξήγηση Κώδικα
    •jobCommander.c
        Εφόσον έχουν δοθεί σωστά arguments, ο commander συνδέεται στον ήδη υπάρχον server, όπως περιγράφεται στην εκφώνηση. Στη συνέχεια, αφού συνδεθεί στο socket, διατρέχει το argv[] από 3 κι έπειτα. Μετράει τα bytes και αποστέλει στον server "[size] [command]".Στη συνέχεια, περιμένει την απάντηση, η οποία θα έρθει στο ίδιο format. Αφού διαβάσει το μέγεθος, κάνει allocate size bytes για να διαβάσει και να εκτυπώσει την απάντηση. Στην περίπτωση που το command ηταν issueJob, περιμένει και δεύτερη φορά για το output του job.
    
    •controllerThread.c
        Στο thread περνάει ως όρισμα το socketFd του πελάτη. Αφού διαβαστεί το μέγεθος και το command με τη διαδικασία που περιγράφηκε προηγουμένως, το thread κάνει lock το bufferSem mutex και προχωράμε στις 5 περιπτώσεις: 
            •Για issueJob, αν jobsInBuffer == bufferSize περιμένει. Αλλιώς γίνεται allocated ένα νέο Job struct, όπου περιέχει την τριάδα του Job και καταχωρείται στον buffer. Επιστρέφεται στον πελάτη μήνυμα επιβεβαίωσης και γίνονται signal τα variables bufferNewJobCond, concurrencyCond. 
            •Για poll, διατρέχει τον buffer και αφού υπολογίσει το συνολικό μέγεθος, τα αποστέλει στον πελάτη.
            •Για setConcurrency, κλειδώνει και τον δεύτερο σημαφόρο, αλλάζει το concurrency variable, και εάν είναι μεγαλύτερο από το προηγούμενο, κάνει broadcast το concurrencyCond. Στη συνέχεια επιστρέφει επιβεβαίωση στον πελάτη.
            •Για stop, αφαιρεί το jobID από το Buffer, αν βρεθεί, επιστρέφει το αντίστοιχο μήνυμα στον πελάτη. Αν έχει αφαιρεθεί κάποια δουλειά, κάνει signal το bufferSpotCond.
            •Για exit, επιστρέφει στον πελάτη το μήνυμα SERVER TERMINATED, και κάνει shutdown το socket της accept. Αυτό θα το διαχειριστεί το main thread για το κλείσιμο του server.
            •Σε κάθε άλλο input, επιστρέφεται στον πελάτη UNKNOWN COMMAND και η εντολή αγνοείται.
    
    •workerThread.c
        Στο workerThread δεν δίνεται κάποιο όρισμα. Αφού ελεγχθούν τα condition vars και το flag serverExit, το thread παίρνει την πρώτη δουλειά του buffer και "κατεβάζει" όλες τις υπόλοιπες κατά μία θέση. Στη συνέχεια, καλεί τη fork(). Το παιδί εκτελεί τη δουλειά μέσω της exec και με τη dup2(), οπως περιγράφεται και στην εκφώνηση, τοποθετεί το output(ή το error) στο αρχείο pid.output. Ο γονιός, περιμένει να τερματίσει το παιδί. Στη συνέχεια, ανοίγει το αρχείο, το διαβάζει, και γράφει το output στο socket. Πριν και μετά το output, γράφει τα αναγνωριστικά, όπως περιγράφονται. Στο τέλος, αποδεσμεύει τη μνήμη, κλείνει το socket και αφού πάρει το σημαφόρο του concurrency, μειώνει κατά 1 τα runningJobs.

    •jobExecutorServer.c
        Ο σέρβερ ξεκινάει και αφού ελέγξει τα arguments, στήνει το connection. Δεσμέυει μνήμη για το Buffer μεγέθους Job * bufferSize(όρισμα). Κάνει initialize τα Mutexes και τα Condition Vars. Στη συνέχεια, δημιουργεί threadPoolSize worker Threads και μπαίνει στο loop. Μέσα στο loop, κάνει accept συνδέσεις. Αν η accept αποτύχει, ελέγχεται το flag serverExit. Αν είναι, κάνει break και προχωράει σε shutdown. Αν η accept πετύχει, δημιουργεί ένα controller thread το οποίο διαχειρίζεται τον commander. Στο shutdown routine, γίνεται broadcast η μεταβλητή concurrencyCond για να ξεμπλοκάρουν οι workers και να επιστρέψουν. Στη συνέχεια, αποδεσμεύεται η μνήμη για τα worker_threads. Έπειτα, σε όσες δουλειές περίμεναν στον buffer, επιστρέφεται το μήνυμα τερματισμού του server και αποδεσμέυεται η μνήμη που κρατούσαν. Τέλος, αποδεσμεύεται και το buffer και ο server τερματίζει.


# Σχόλια
    Στην εργασία υπάρχουν ορισμένα memory leaks. Δεν έχουν γίνει εξαντλητικοί έλεγχοι σε edge cases λόγω χρόνου.




