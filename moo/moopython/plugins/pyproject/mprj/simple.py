import moo
import os.path
import shutil
from moo.utils import _

from mprj.project import Project
from mprj.config import Config, Dict
from mprj.settings import Filename
from mprj.utils import print_error
from mprj.session import Session


class SimpleConfig(Config):
    __items__ = {
        'vars' : Dict(str, name=_('Environment variables')),
        'file_selector_dir' : Filename,
    }


class SimpleProject(Project):

    __config__ = SimpleConfig

    def init_ui(self):
        Project.init_ui(self)

        self.filesel = None
        self.filesel_merge_id = 0
        self.__filesel_cb_id = 0

        plugin = moo.edit.plugin_lookup('FileSelector')
        if plugin:
            try:
                self.filesel = plugin.call_method('get-widget', self.window)
                if self.filesel:
                    last_dir = self.config.file_selector_dir
                    if not last_dir:
                        last_dir = self.topdir
                    self.filesel.chdir(last_dir)
                    self.__filesel_cb_id = self.filesel.connect('notify::current-directory', self.__filesel_cb)
            except:
                print_error()

    def __filesel_cb(self, filesel, *whatever):
        self.config.file_selector_dir = filesel.get_property('current-directory')

    def deinit_ui(self):
        if self.__filesel_cb_id:
            self.filesel.disconnect(self.__filesel_cb_id)
            self.__filesel_cb_id = 0
        if self.filesel and self.filesel_merge_id:
            xml = self.filesel.get_ui_xml()
            xml.remove_ui(self.filesel_merge_id)
        self.filesel = None
        self.filesel_merge_id = 0
        Project.deinit_ui(self)

    def load(self):
        Project.load(self)
        self.load_session()

    def close(self):
        self.save_session()
        self.save_config()
        return self.window.close_all()

    def get_session_file(self):
        return os.path.join(self.topdir, '.' + \
                            os.path.basename(self.filename) + '.session')

    def load_session(self):
        try:
            file = self.get_session_file()
            if os.path.exists(file):
                Session(file).attach(self.window)
        except:
            print_error()

    def save_session(self):
        try:
            file = self.get_session_file()
            Session(self.window).save(file)
        except:
            print_error()

    def save_config(self):
        try:
            tmpname = os.path.join(os.path.dirname(self.filename), '.' + os.path.basename(self.filename) + '.tmp')
            tmp = open(tmpname, "w")
            tmp.write(self.config.format())
            tmp.close()
            shutil.copymode(self.filename, tmpname)
            shutil.move(self.filename, self.filename + '.bak')
            shutil.move(tmpname, self.filename)
        except:
            print_error()

    def options_dialog(self, window):
        dialog = self.create_options_dialog()
        dialog.run(window)

    def create_options_dialog(self):
        return None


if __name__ == '__main__':
    from mprj.config import File

    s1 = """
    <medit-project name="moo" type="Simple" version="2.0">
      <vars>
        <foo>bar</foo>
        <blah>bom</blah>
      </vars>
    </medit-project>
    """

    c = SimpleConfig(File(s1))
    s2 = str(c.get_xml())

    print s2

    c = SimpleConfig(File(s2))
    s3 = str(c.get_xml())

    assert s2 == s3
