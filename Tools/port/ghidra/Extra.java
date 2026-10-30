// Schreibt Listings für RVAs ohne .pdata (Blattfunktionen), folgt dem Fluss bis RET/unbedingtem JMP. Argumente: Eingabe, Ausgabe
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.io.*;
import java.nio.file.*;
import java.util.*;
public class Extra extends GhidraScript {
  @Override public void run() throws Exception {
    String[] a = getScriptArgs();
    Address base = currentProgram.getImageBase();
    Listing l = currentProgram.getListing();
    try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(a[1])))) {
      for (String s : Files.readAllLines(Paths.get(a[0]))) {
        s = s.trim(); if (s.isEmpty()) continue;
        long r = Long.parseLong(s, 16);
        Address p = base.add(r);
        Instruction ins = l.getInstructionAt(p);
        if (ins == null) { disassemble(p); ins = l.getInstructionAt(p); } if (ins == null) continue;
        StringBuilder sb = new StringBuilder(); long end = r; int n = 0;
        TreeSet<Long> pending = new TreeSet<>();
        Address cur = p;
        while (ins != null && n++ < 600) {
          long v = ins.getAddress().subtract(base);
          sb.append(Long.toHexString(v).toUpperCase()).append(' ').append(ins.toString());
          for (Reference x : ins.getReferencesFrom())
            if (x.getToAddress().isMemoryAddress()) {
              long t = x.getToAddress().subtract(base);
              sb.append(" ;").append(Long.toHexString(t).toUpperCase());
              if (ins.getFlowType().isConditional() && t > v && t - r < 0x2000) pending.add(t);
            }
          sb.append('\n');
          end = v + ins.getLength();
          boolean stop = !ins.getFlowType().hasFallthrough();
          Address nx = ins.getAddress().add(ins.getLength());
          if (stop) {
            Long q = pending.ceiling(end);
            if (q == null) break;
            nx = base.add(q);
          }
          ins = l.getInstructionAt(nx);
        }
        w.printf("F %X %X%n", r, end); w.print(sb);
      }
    }
  }
}
