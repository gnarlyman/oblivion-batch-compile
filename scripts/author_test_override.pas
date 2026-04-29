{
  author_test_override.pas

  Authors a SCPT-as-override into a target plugin and tweaks SCTX so the
  bytecode (SCDA) becomes stale relative to the source. Used as a setup
  step before exercising the oblivion-batch-compile plugin.

  Args:
    --target-plugin=<filename>   required
    --master-formid=<hex>        required (e.g. 1A012EAF)

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
  MasterFid: cardinal;
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
    AddMessage('ERROR: target plugin not found in load order: ' + TargetName);
    Result := 2;
    Exit;
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
  // Apply the real PSMQD DLC-guard fix: extend the IsModLoaded check so
  // Reborn's "Delay DLC Start.esp" satisfies it too. This is a genuine
  // semantic change, so the recompiled SCDA must differ from the master's.
  NewSCTX := StringReplace(OldSCTX,
    'IsModLoaded "Oblivion DLC Delayers.esp" == 1',
    'IsModLoaded "Oblivion DLC Delayers.esp" == 1 || IsModLoaded "Delay DLC Start.esp" == 1',
    [rfReplaceAll]);

  if NewSCTX = OldSCTX then begin
    AddMessage('WARNING: pattern not found in SCTX — no change applied');
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
