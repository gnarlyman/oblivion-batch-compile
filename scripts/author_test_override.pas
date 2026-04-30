{
  author_test_override.pas

  Authors a SCPT-as-override into a target plugin and applies a
  StringReplace transformation to its SCTX so the bytecode (SCDA)
  becomes stale relative to the source. Used as a setup step before
  exercising the oblivion-batch-compile plugin.

  Args:
    --target-plugin=<filename>   required
    --master-formid=<hex>        required (e.g. 1A012EAF)
    --sctx-file=<path>           optional. Path to a text file whose
                                 contents become the override's SCTX.
                                 Wholesale replacement — preferred for
                                 source-controlled patches.
    --sctx-find=<text>           optional. Substring to search for.
    --sctx-replace=<text>        optional. Replacement substring.

  Mode precedence: --sctx-file > --sctx-find/--sctx-replace > built-in
  PSMQD DLC-guard fix (the original test default).

  Both find and replace strings may contain spaces — pass each as a
  single-quoted argument with internal double-quotes so MO2's
  args.join doesn't split them, e.g. --sctx-find='"IsModLoaded ..."'.

  IMPORTANT: do not place curly braces inside this block comment.
}
unit OBCAuthor;

const
  TOOL_VERSION = '0.1.0';

function GetArg(const key: string): string;
var
  i: integer;
  prefix, s: string;
begin
  Result := '';
  prefix := '--' + key + '=';
  for i := 0 to ParamCount do begin
    s := ParamStr(i);
    if Copy(s, 1, Length(prefix)) = prefix then begin
      Result := Copy(s, Length(prefix) + 1, Length(s));
      Exit;
    end;
  end;
end;

function ParseFormID(const s: string): cardinal;
var
  body: string;
begin
  body := s;
  if (Length(body) >= 2) and ((body[2] = 'x') or (body[2] = 'X')) then
    body := Copy(body, 3, Length(body));
  Result := StrToInt('$' + body);
end;

function FindFileByName(const name: string): IInterface;
var
  i: integer;
begin
  Result := nil;
  for i := 0 to Pred(FileCount) do
    if SameText(GetFileName(FileByIndex(i)), name) then begin
      Result := FileByIndex(i);
      Exit;
    end;
end;

function LookupByLoFormID(fid: cardinal): IInterface;
var
  loIdx: integer;
  pluginFile: IInterface;
begin
  Result := nil;
  loIdx := (fid shr 24) and $FF;
  if loIdx >= FileCount then Exit;
  pluginFile := FileByLoadOrder(loIdx);
  if not Assigned(pluginFile) then Exit;
  Result := RecordByFormID(pluginFile, fid, False);
  if not Assigned(Result) then
    Result := RecordByFormID(pluginFile, fid, True);
end;

function Initialize: integer;
var
  TargetPlugin, MasterRec, Override, SCTXElem: IInterface;
  TargetName, FormIDStr, MasterPluginName, OldSCTX, NewSCTX: string;
  SctxFile, SctxFind, SctxReplace: string;
  MasterFid: cardinal;
  sl: TStringList;
begin
  TargetName := GetArg('target-plugin');
  FormIDStr := GetArg('master-formid');

  if (TargetName = '') or (FormIDStr = '') then begin
    AddMessage('ERROR: missing args. need --target-plugin=NAME --master-formid=HEX');
    Result := 1;
    Exit;
  end;

  AddMessage('OBCAuthor v' + TOOL_VERSION);
  AddMessage('Target: ' + TargetName + ', master FormID: ' + FormIDStr);

  TargetPlugin := FindFileByName(TargetName);
  if not Assigned(TargetPlugin) then begin
    AddMessage('Target plugin not in load order; creating new file: ' + TargetName);
    TargetPlugin := AddNewFileName(TargetName);
    if not Assigned(TargetPlugin) then begin
      AddMessage('ERROR: AddNewFileName returned nil for: ' + TargetName);
      Result := 2;
      Exit;
    end;
  end;

  MasterFid := ParseFormID(FormIDStr);
  MasterRec := LookupByLoFormID(MasterFid);
  if not Assigned(MasterRec) then begin
    AddMessage('ERROR: master record not found for FormID ' + FormIDStr);
    Result := 3;
    Exit;
  end;

  MasterPluginName := GetFileName(GetFile(MasterRec));
  AddMessage('Master record: ' + EditorID(MasterRec) + ' from ' + MasterPluginName);

  AddMasterIfMissing(TargetPlugin, MasterPluginName);

  Override := wbCopyElementToFile(MasterRec, TargetPlugin, False, True);
  if not Assigned(Override) then begin
    AddMessage('ERROR: wbCopyElementToFile returned nil');
    Result := 4;
    Exit;
  end;

  SCTXElem := ElementByPath(Override, 'SCTX');
  if not Assigned(SCTXElem) then begin
    AddMessage('ERROR: override has no SCTX element');
    Result := 5;
    Exit;
  end;

  OldSCTX := GetEditValue(SCTXElem);

  SctxFile := GetArg('sctx-file');
  if SctxFile <> '' then begin
    if not FileExists(SctxFile) then begin
      AddMessage('ERROR: --sctx-file not found: ' + SctxFile);
      Result := 6;
      Exit;
    end;
    sl := TStringList.Create;
    try
      sl.LoadFromFile(SctxFile);
      AddMessage('Loaded: ' + IntToStr(sl.Count) + ' lines');
      // TStringList.Text re-emits with platform line endings. CS accepts
      // CRLF in SCTX so pass it through. (Avoid char-indexing trim — JvI
      // doesn't support s[i].)
      NewSCTX := sl.Text;
    finally
      sl.Free;
    end;
    AddMessage('SCTX replaced from file: ' + SctxFile + ' (' + IntToStr(Length(NewSCTX)) + ' chars)');
  end else begin
    SctxFind := GetArg('sctx-find');
    SctxReplace := GetArg('sctx-replace');
    if SctxFind = '' then begin
      // Default: the PSMQD DLC-guard fix.
      SctxFind := 'IsModLoaded "Oblivion DLC Delayers.esp" == 1';
      SctxReplace := 'IsModLoaded "Oblivion DLC Delayers.esp" == 1 || IsModLoaded "Delay DLC Start.esp" == 1';
      AddMessage('No --sctx-file or --sctx-find; using built-in PSMQD DLC-guard fix');
    end;

    AddMessage('SCTX find:    ' + SctxFind);
    AddMessage('SCTX replace: ' + SctxReplace);

    NewSCTX := StringReplace(OldSCTX, SctxFind, SctxReplace, [rfReplaceAll]);

    if NewSCTX = OldSCTX then begin
      AddMessage('WARNING: --sctx-find pattern not found in SCTX — no change applied');
    end;
  end;

  SetEditValue(SCTXElem, NewSCTX);

  AddMessage('Override authored. Old SCTX length: ' + IntToStr(Length(OldSCTX)) +
             ', new: ' + IntToStr(Length(NewSCTX)));
  AddMessage('OK');
  Result := 0;
end;

function Process(e: IInterface): integer;
begin
  Result := 0;
end;

function Finalize: integer;
begin
  Result := 0;
end;

end.
