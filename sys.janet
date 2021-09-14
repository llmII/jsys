(import csys :as sys)

# helpers ********************************************************************
# Definition from:
#   https://github.com/janet-lang/spork/blob/master/spork/path.janet
# License: MIT
# Copyright: 2019 Calvin Rose
# Overall Copyright: 2020 Calvin Rose and contributors
(defn- redef
  "Redef a value, keeping all metadata."
  [from to]
  (setdyn (symbol to) (dyn (symbol from))))

(defn- redef-clone
  "Redef a value, keeping all metadata."
  [from to]
  (setdyn (symbol to) (table/clone (dyn (symbol from)))))

(defn- redef-
  "Redef a value, then set it private in it's metadata."
  [from to]
  (redef-clone from to)
  (set ((dyn (symbol to)) :private) true))

(defn- redef+
  "Redef a value, then set it private in it's metadata."
  [from to]
  (redef-clone from to)
  (set ((dyn (symbol to)) :private) false))

(defmacro- redef-symbol
  "Redef, minus the string conversions/usage."
  [from to]
  ~(redef+ ,(string from) ',to))

# TODO: redef+ on first, redef on rest - don't clone so many tables!
(defn- redef-multi*
  ``
  Helper for redef-multi, does redefs over an indexed collection. Makes each
  new symbol exported.
  ``
  [from to]
  (match (length to)
    0   (redef+ from to)
    _   (each n to (redef+ (string from) n))))

(defmacro- redef-multi
  "Redef each symbol after the first as the first."
  [from & to]
     (var i 0)
     (let [len        (length to)
           collecting @[]]
       (while (< i len)
         (array/push collecting (get to i))
         (++ i))
       ~(redef-multi* ,(string from) ',collecting)))

# All the system specific functions exported from C
(def- exports '(chdir chown chroot dup2 fork setegid seteuid setgid setuid
                      setsid fcntl getpwnam getgrnam))

# Figuring out which OS and calling the correct function in every wrapper
# would be super tedious, so create a local definition prefixed with '_'
(let [os (match (os/which)
           :windows 'windows
           _        'nix)]
  (each binding exports
    (redef- (string "sys/" os "/" binding) (string "_" binding))))

# chdir - change directory ***************************************************
(redef-multi _chdir chdir change-directory)

# chown - change fs entry ownership ******************************************
# TODO: support chown with username/groupname instead of just uid/gid, support
#   optional uid/gid (use keyword args).
(redef-multi _chown chown change-owner)

# chroot - change what is considered / ***************************************
(redef-multi _chroot chroot change-root)

# dup2 - reassign a fd's descriptor ******************************************
# TODO: Easier file redirection supporting the :out and :in keywords for
#   stdout and stdin; document flags; Make overall nicer/easier.
(redef-multi _dup2 dup2 redirect-file)

# fork - split off into 2 processes ******************************************
(redef-symbol _fork fork)

# setegid - set effective operating group id *********************************
# TODO: Allow for setting of the gid by group name
(redef-multi _setegid setegid set-effective-group)

# seteuid - set effective operating group id *********************************
# TODO: Allow for setting of the uid by user name
(redef-multi _seteuid seteuid set-effective-user)

# setgid - set operating group id ********************************************
# TODO: Allow for setting of the gid by group name
(redef-multi _setgid setgid set-group)

# setuid - set operating user id *********************************************
# TODO: Allow for setting of the uid by user name
(redef-multi _setuid setuid set-user)

# setsid - create new session ************************************************
(redef-multi _setsid setsid new-session)

# fcntl - file settings ******************************************************
# TODO: provide a nicer way to use this, right now we're only supporting
#   locks but when we support more would be nice to have a better interface
(redef-multi _fcntl fcntl file-settings)

# getpwnam - get user by name or id ******************************************
(redef-multi _getpwnam getpwnam get-user-info)

# getgrnam - get group by name or id *****************************************
(redef-multi _getgrnam getgrnam get-group-info)

# TODO: provide easier lockfile interface
