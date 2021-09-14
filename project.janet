(declare-project
 :name "jsys"
 :author "llmII <dev@amlegion.org>"
 :license "ISC"
 :url "https://github.com/llmII/jsys"
 :repo "git+https://github.com/llmII/jsys.git"
 :description "System level utilities/functions for Janet.")

(declare-source
 :source ["sys.janet"])

(declare-native
 :name "csys"
 :source ["src/csys.c"])
