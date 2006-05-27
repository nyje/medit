import moo
import gobject

PLUGIN_ID = "TestPythonPlugin"

class Action(moo.edit.Action):
    def __init__(self):
        moo.edit.Action.__init__(self)
        self.set_property("label", "AnAction")
        #print "__init__"

    def do_check_state(self):
        #print "check_state"
        return True

    def do_activate(self):
        #print "activate: doc is", self.doc
        pass


class Plugin(moo.edit.Plugin):
    def __init__(self):
        moo.edit.Plugin.__init__(self)

        self.set_info(moo.edit.PluginInfo("RealPythonPlugin", "Real Python Plugin",
                                          description="Plugin", author="Uknown",
                                          version="3.1415926"))

    def do_init(self):
        #print "do_init"
        return True

    def do_deinit(self):
        #print "do_deinit"
        pass

    def do_attach_win(self, window):
        #print "do_attach_win"
        pass
    def do_detach_win(self, window):
        #print "do_detach_win"
        pass
    def do_attach_doc(self, window):
        #print "do_attach_doc"
        pass
    def do_detach_doc(self, window):
        #print "do_detach_doc"
        pass


gobject.type_register(Action)
gobject.type_register(Plugin)
moo.edit.plugin_register(Plugin)
