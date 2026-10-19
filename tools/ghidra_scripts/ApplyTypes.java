// Parses a C header (symbols/types.h) and adds its types to the program,
// replacing earlier versions so the script can be re-run as types evolve.
// Usage (headless postScript): ApplyTypes.java <types.h>
//@category Xbox

import java.io.File;
import java.nio.file.Files;
import java.util.Iterator;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParser;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeConflictHandler;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.StandAloneDataTypeManager;

public class ApplyTypes extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String src = Files.readString(new File(getScriptArgs()[0]).toPath());
		StandAloneDataTypeManager tmp = new StandAloneDataTypeManager("types.h");
		int tx = tmp.startTransaction("parse");
		CParser p = new CParser(tmp, true, null);
		p.parse(src);
		tmp.endTransaction(tx, true);

		DataTypeManager dtm = currentProgram.getDataTypeManager();
		int n = 0;
		Iterator<DataType> it = tmp.getAllDataTypes();
		while (it.hasNext()) {
			DataType dt = it.next();
			if (dt.getCategoryPath().isRoot()) {
				dtm.addDataType(dt, DataTypeConflictHandler.REPLACE_HANDLER);
				n++;
			}
		}
		tmp.close();
		println("ApplyTypes: " + n + " types");
	}
}
