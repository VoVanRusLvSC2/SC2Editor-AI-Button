const contextPrefix='[Контекст SC2 Editor]';
const userMarker='[Сообщение пользователя]\n';

// The provider sometimes uses the hidden editor context as a chat title.
// Never display that context; extract only the user's original message.
export function displayUserText(value){
  const text=String(value||'').trimStart();
  if(!text.startsWith(contextPrefix))return text;
  const marker=text.indexOf(userMarker);
  return marker<0?'':text.slice(marker+userMarker.length);
}
export function isContextTitle(value){return String(value||'').trimStart().startsWith(contextPrefix);}
export function compactTitle(value){return displayUserText(value).replace(/\s+/g,' ').trim().slice(0,90);}
export function chatTitle(name,preview,fallback){
  return compactTitle(name)||compactTitle(preview)||fallback;
}
export function firstUserTitle(turns){
  for(const turn of turns||[])for(const item of turn.items||[]){
    if(item.type!=='userMessage')continue;
    const text=(item.content||[]).map(part=>part.text||'').join('');
    const title=compactTitle(text);if(title)return title;
  }
  return '';
}
