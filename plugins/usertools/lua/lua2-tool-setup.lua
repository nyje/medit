local _g = getfenv(0)
local medit = require("medit")
_g.app = medit.get_app_obj()
_g.editor = _g.app.editor
_g.doc = _g.app.active_document
_g.view = _g.app.active_view
_g.window = _g.app.active_window
setfenv(0, _g)
