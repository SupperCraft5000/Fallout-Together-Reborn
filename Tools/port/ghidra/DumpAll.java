// Disassembliert alle .pdata-Funktionen (ohne Auto-Analyse), legt Funktionen an, schreibt eine Text-Dumpdatei.
// Argument: Ausgabedatei
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.Reference;
import java.io.*;
import java.util.*;

public class DumpAll extends GhidraScript {
  @Override public void run() throws Exception {
    String out = getScriptArgs()[0];
    Memory mem = currentProgram.getMemory();
    Address base = currentProgram.getImageBase();
    MemoryBlock pd = mem.getBlock(".pdata");
    long n = pd.getSize() / 12;
    TreeMap<Long,Long> f = new TreeMap<>();
    for (long i = 0; i < n; i++) {
      Address a = pd.getStart().add(i * 12);
      long b = mem.getInt(a) & 0xffffffffL, e = mem.getInt(a.add(4)) & 0xffffffffL;
      if (b == 0) continue;
      Long o = f.get(b);
      if (o == null || e > o) f.put(b, e);
    }
    println("pdata " + f.size());
    Listing l = currentProgram.getListing();
    int c = 0;
    for (var en : f.entrySet()) {
      Address s = base.add(en.getKey());
      if (l.getInstructionAt(s) == null) {
        DisassembleCommand cmd = new DisassembleCommand(new AddressSet(s, base.add(en.getValue() - 1)), null, true);
        cmd.enableCodeAnalysis(false);
        cmd.applyTo(currentProgram);
      }
      if (++c % 20000 == 0) { println("dis " + c); }
    }
    c = 0;
    try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(out), 1 << 20))) {
      for (var en : f.entrySet()) {
        long b = en.getKey(), e = en.getValue();
        Address s = base.add(b);
        w.printf("F %X %X%n", b, e);
        InstructionIterator it = l.getInstructions(new AddressSet(s, base.add(e - 1)), true);
        while (it.hasNext()) {
          Instruction ins = it.next();
          StringBuilder sb = new StringBuilder(Long.toHexString(ins.getAddress().subtract(base)).toUpperCase()).append(' ').append(ins.toString());
          // Rip-Ziele/Sprungziele als RVA anhaengen
          for (Reference r : ins.getReferencesFrom()) {
            if (r.getToAddress().isMemoryAddress()) sb.append(" ;").append(Long.toHexString(r.getToAddress().subtract(base)).toUpperCase());
          }
          w.println(sb);
        }
        if (++c % 20000 == 0) println("dump " + c);
      }
    }
    c = 0;
    if (getScriptArgs().length > 1) {
      for (var en : f.entrySet()) {
        Address s = base.add(en.getKey());
        if (getFunctionAt(s) == null) { try { createFunction(s, null); } catch (Exception x) {} }
        if (++c % 20000 == 0) println("fn " + c);
      }
    }
  }
}
