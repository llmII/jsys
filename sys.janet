(import csys :as sys)
(use jumble)

# All the system specific functions exported from C
(def- exports '(chown chroot dup2 fileno fork setegid seteuid setgid setuid
                      setsid fcntl getpwnam getgrnam strftime getpid getppid))

# Figuring out which OS and calling the correct function in every wrapper
# would be super tedious, so create a local definition prefixed with '_'
(let [os (match (os/which)
           :windows 'windows
           _        'nix)]
  (each binding exports
    (do
      (defclone+* (symbol "sys/" os "/" binding) (symbol os "/" binding))
      (defclone-* (symbol "sys/" os "/" binding) (symbol "_" binding)))))

# chown - change fs entry ownership ******************************************
# TODO: support chown with username/groupname instead of just uid/gid, support
#   optional uid/gid (use keyword args).
(defaliases _chown chown change-owner :export true)

# chroot - change what is considered / ***************************************
(defaliases _chroot chroot change-root :export true)

# dup2 - reassign a fd's descriptor ******************************************
# TODO: Easier file redirection supporting the :out and :in keywords for
#   stdout and stdin; document flags; Make overall nicer/easier.
(defaliases _dup2 dup2 redirect-file :export true)

# fork - split off into 2 processes ******************************************
# TODO: may need a different idea on *BSD where kqueue is dead in child forks
(defaliases _fork fork :export true)

# setegid - set effective operating group id *********************************
# TODO: Allow for setting of the gid by group name
(defaliases _setegid setegid set-effective-group :export true)

# seteuid - set effective operating group id *********************************
# TODO: Allow for setting of the uid by user name
(defaliases _seteuid seteuid set-effective-user :export true)

# setgid - set operating group id ********************************************
# TODO: Allow for setting of the gid by group name
(defaliases _setgid setgid set-group :export true)

# setuid - set operating user id *********************************************
# TODO: Allow for setting of the uid by user name
(defaliases _setuid setuid set-user :export true)

# setsid - create new session ************************************************
(defaliases _setsid setsid new-session :export true)

# fcntl - file settings ******************************************************
# TODO: provide a nicer way to use this, right now we're only supporting
#   locks but when we support more would be nice to have a better interface
(defaliases _fcntl fcntl file-settings :export true)

# getpwnam - get user by name or id ******************************************
(defaliases _getpwnam getpwnam get-user-info :export true)

# getgrnam - get group by name or id *****************************************
(defaliases _getgrnam getgrnam get-group-info :export true)

# strftime - get a formatted time string *************************************
(defaliases _strftime strftime date-string :export true)

# getpid - get current process id ********************************************
(defaliases _getpid getpid :export true)
#
# getppid - get the process id of the parent of the current process **********
(defaliases _getppid getppid :export true)
# TODO: provide easier lockfile interface
