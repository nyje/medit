require("munit")

function test_undo()
  doc = editor.new_doc()
  tassert_eq(doc.get_text(), '')
  doc.begin_user_action()
  doc.append_text('blah')
  tassert_eq(doc.get_text(), 'blah')
  doc.append_text('halb')
  tassert_eq(doc.get_text(), 'blahhalb')
  doc.end_user_action()
  tassert_eq(doc.can_redo(), false)
  tassert_eq(doc.can_undo(), true)
  doc.undo()
  tassert_eq(doc.can_redo(), true)
  tassert_eq(doc.can_undo(), false)
  tassert_eq(doc.get_text(), '')
  doc.redo()
  tassert_eq(doc.get_text(), 'blahhalb')
  tassert_eq(doc.can_redo(), false)
  tassert_eq(doc.can_undo(), true)

  doc.begin_non_undoable_action()
  doc.set_text('foobar')
  doc.end_non_undoable_action()
  tassert_eq(doc.get_text(), 'foobar')
  tassert_eq(doc.can_redo(), false)
  tassert_eq(doc.can_undo(), false)
  doc.undo()
  tassert_eq(doc.get_text(), 'foobar')
  tassert_eq(doc.can_redo(), false)
  tassert_eq(doc.can_undo(), false)
  doc.redo()
  tassert_eq(doc.get_text(), 'foobar')
  tassert_eq(doc.can_redo(), false)
  tassert_eq(doc.can_undo(), false)

  doc.set_modified(false)
  doc.close()
end

function test_edit()
  doc = editor.new_doc()
  tassert_eq(doc.get_text(), '')
  doc.set_text('foo')
  tassert_eq(doc.get_text(), 'foo')
  doc.clear()
  tassert_eq(doc.get_text(), '')
  doc.set_text('foo')
  doc.select_all()
  doc.copy()
  doc.paste()
  tassert_eq(doc.get_text(), 'foo')
  doc.paste()
  tassert_eq(doc.get_text(), 'foofoo')
  doc.select_all()
  doc.cut()
  tassert_eq(doc.get_text(), '')
  doc.paste()
  tassert_eq(doc.get_text(), 'foofoo')

  doc.set_modified(false)
  doc.close()
end

test_undo()
test_edit()
